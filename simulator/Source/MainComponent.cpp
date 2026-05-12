// MainComponent.cpp
#include "MainComponent.h"
#include <cmath>

static constexpr int kTransportH    = 44;
static constexpr int kStatusH       = 24;
static constexpr int kFilterBarH    = 28;
static constexpr int kDebugPanelW   = 520;
static constexpr int kStripW        = 75;   // width per channel strip
static constexpr int kWindowW       = 1850;
static constexpr int kWindowH       = 550;

static const juce::StringArray kFilterNames = { "All", "Audio", "Instrument", "Bus", "FX", "VCA", "Master" };

MainComponent::MainComponent()
    : transport(uiBus)
{
    std::memset(lcdBuffer, ' ', sizeof(lcdBuffer));

    for (int i = 0; i < kTotalChannels; ++i)
        channelTypes[i] = "Audio";

    // --- Channel strips inside a scrollable container ---
    for (int i = 0; i < kTotalChannels; ++i)
    {
        auto strip = std::make_unique<SimChannelStrip>(i, uiBus);
        stripContainer.addAndMakeVisible(*strip);
        strips.push_back(std::move(strip));
    }

    stripViewport.setViewedComponent(&stripContainer, false);
    stripViewport.setScrollBarsShown(false, true);  // horizontal scroll only
    addAndMakeVisible(stripViewport);

    // --- Transport ---
    addAndMakeVisible(transport);

    // --- VCA container ---
    addAndMakeVisible(vcaContainer);

    // --- Status ---
    addAndMakeVisible(statusLabel);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    statusLabel.setFont(juce::Font(11.0f));

    // --- Filter buttons ---
    for (int i = 0; i < kFilterNames.size(); ++i)
    {
        auto btn = std::make_unique<juce::TextButton>(kFilterNames[i]);
        btn->setClickingTogglesState(false);
        btn->onClick = [this, i]
        {
            if (i == 0)
                setActiveFilter("");
            else
                setActiveFilter(kFilterNames[i]);
        };
        addAndMakeVisible(*btn);
        filterButtons.push_back(std::move(btn));
    }
    filterButtons[0]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff005588));

    // --- Debug toggle ---
    addAndMakeVisible(debugToggle);
    debugToggle.setClickingTogglesState(true);
    debugToggle.onClick = [this]
    {
        debugVisible = debugToggle.getToggleState();
        debugPanel.setVisible(debugVisible);

        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            int newW = debugVisible ? kWindowW + kDebugPanelW : kWindowW;
            setSize(newW, kWindowH);
            window->centreWithSize(newW, kWindowH);
        }
        else
        {
            int newW = debugVisible ? kWindowW + kDebugPanelW : kWindowW;
            setSize(newW, kWindowH);
        }

        resized();
    };

    addChildComponent(debugPanel);

    // --- Subscribe to UI bus ---
    uiBus.subscribe([this](const Sim::Message& msg) { handleUiMessage(msg); });

    // --- Connect to middleware ---
    client.start("127.0.0.1", 8888, [this](const Sim::Message& msg)
    {
        handleServerMessage(msg);
    });

    startTimerHz(2);
    setSize(kWindowW, kWindowH);
}

MainComponent::~MainComponent()
{
    client.stop();
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    if (debugVisible)
        debugPanel.setBounds(bounds.removeFromRight(kDebugPanelW));

    bounds = bounds.reduced(4);

    // Status bar + debug toggle
    auto topBar = bounds.removeFromTop(kStatusH);
    debugToggle.setBounds(topBar.removeFromRight(60));
    statusLabel.setBounds(topBar);

    // Filter bar
    auto filterBar = bounds.removeFromTop(kFilterBarH);
    int btnW = filterBar.getWidth() / (int)filterButtons.size();
    for (auto& btn : filterButtons)
        btn->setBounds(filterBar.removeFromLeft(btnW).reduced(2, 2));

    // Transport at bottom
    transport.setBounds(bounds.removeFromBottom(kTransportH));
    bounds.removeFromBottom(4);

    // VCA faders on the right (same height as channel strips)
    if (!vcaStrips.empty())
    {
        int vcaW = kStripW * (int)vcaStrips.size();
        auto vcaArea = bounds.removeFromRight(vcaW);
        vcaContainer.setBounds(vcaArea);

        int stripW = vcaArea.getWidth() / (int)vcaStrips.size();
        int x = 0;
        for (auto& vca : vcaStrips)
        {
            vca->setBounds(x, 0, stripW, vcaArea.getHeight());
            x += stripW;
        }
        bounds.removeFromRight(4);  // gap between VCAs and channels
    }
    else
    {
        vcaContainer.setBounds(0, 0, 0, 0);
    }

    // Channel strip viewport fills the rest
    stripViewport.setBounds(bounds);
    layoutStrips();
}

void MainComponent::layoutStrips()
{
    // Count visible strips
    int visibleCount = 0;
    for (int i = 0; i < kTotalChannels; ++i)
        if (strips[i]->isVisible()) ++visibleCount;

    int viewH = stripViewport.getHeight();
    int totalW = visibleCount * kStripW;

    // If all visible strips fit in the viewport, expand them to fill
    int availW = stripViewport.getWidth();
    int stripWidth = (visibleCount > 0 && totalW <= availW)
                         ? availW / visibleCount
                         : kStripW;

    int actualTotalW = visibleCount * stripWidth;
    stripContainer.setSize(std::max(actualTotalW, availW), viewH);

    int x = 0;
    for (int i = 0; i < kTotalChannels; ++i)
    {
        if (strips[i]->isVisible())
        {
            strips[i]->setBounds(x, 0, stripWidth, viewH);
            x += stripWidth;
        }
    }
}

void MainComponent::timerCallback()
{
    if (client.isConnected())
    {
        statusLabel.setText("Connected", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        statusLabel.setText("Disconnected - retrying...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }
}

void MainComponent::setActiveFilter(const juce::String& filter)
{
    activeFilter = filter;

    for (int i = 0; i < (int)filterButtons.size(); ++i)
    {
        bool active = (i == 0 && filter.isEmpty()) ||
                      (i > 0 && kFilterNames[i] == filter);
        filterButtons[i]->setColour(juce::TextButton::buttonColourId,
                                    active ? juce::Colour(0xff005588)
                                           : juce::LookAndFeel::getDefaultLookAndFeel()
                                                 .findColour(juce::TextButton::buttonColourId));
    }

    applyFilter();
    layoutStrips();
}

void MainComponent::applyFilter()
{
    for (int i = 0; i < kTotalChannels; ++i)
    {
        if (activeFilter.isEmpty())
        {
            strips[i]->setVisible(true);
        }
        else
        {
            juce::String type = channelTypes[i].isEmpty() ? "Audio" : channelTypes[i];
            strips[i]->setVisible(type == activeFilter);
        }
    }
}

void MainComponent::handleUiMessage(const Sim::Message& msg)
{
    switch (msg.type)
    {
        case Sim::MsgType::FaderMove:
        case Sim::MsgType::FaderTouch:
        case Sim::MsgType::ButtonPress:
        case Sim::MsgType::VPotTurn:
        case Sim::MsgType::VcaFaderMove:
            client.send(msg);
            break;
        default:
            break;
    }
}

void MainComponent::handleServerMessage(const Sim::Message& msg)
{
    debugPanel.logMessage(msg);

    switch (msg.type)
    {
        case Sim::MsgType::FaderUpdate:
            if (msg.faderId >= 0 && msg.faderId < kTotalChannels)
                strips[msg.faderId]->setFaderValue(msg.value);
            break;

        case Sim::MsgType::VPotRingUpdate:
            if (msg.vpotId >= 0 && msg.vpotId < kTotalChannels)
                strips[msg.vpotId]->setVPotRing(msg.vpotMode, msg.vpotPosition);
            break;

        case Sim::MsgType::ButtonLedUpdate:
        {
            int note = msg.buttonNote;

            // Channel strip buttons with global encoding:
            // Rec: 0x00+ch, Solo: 0x20+ch, Mute: 0x40+ch, Select: 0x60+ch
            int globalCh = -1;
            if (note >= 0x60 && note < 0x60 + kTotalChannels)
                globalCh = note - 0x60;
            else if (note >= 0x40 && note < 0x40 + kTotalChannels)
                globalCh = note - 0x40;
            else if (note >= 0x20 && note < 0x20 + kTotalChannels)
                globalCh = note - 0x20;
            else if (note >= 0x00 && note < kTotalChannels)
                globalCh = note;

            if (globalCh >= 0 && globalCh < kTotalChannels)
                strips[globalCh]->setButtonLed(note, msg.pressed);

            if (note >= 0x5B && note <= 0x5F)
                transport.setLed(static_cast<Sim::TransportButton>(note), msg.pressed);
            break;
        }

        case Sim::MsgType::LcdUpdate:
        {
            // Middleware sends dB text with lcdOffset = channel index
            int ch = msg.lcdOffset;
            if (ch >= 0 && ch < kTotalChannels)
            {
                juce::String text(msg.lcdText, msg.lcdLength);
                strips[ch]->setLcdText(1, text);
            }
            break;
        }

        case Sim::MsgType::TrackMeta:
        {
            int ch = msg.trackMcuChannel;
            if (ch >= 0 && ch < kTotalChannels)
            {
                juce::String name(msg.trackName);
                juce::String type(msg.trackType);

                strips[ch]->setPluginName(name);
                channelTypes[ch] = type.isEmpty() ? "Audio" : type;

                applyFilter();
                layoutStrips();
            }
            break;
        }

        case Sim::MsgType::MeterUpdate:
        {
            int ch = msg.meterChannel;
            if (ch >= 0 && ch < kTotalChannels)
            {
                float normL = static_cast<float>(msg.meterPeakL) / 16383.0f;
                float normR = static_cast<float>(msg.meterPeakR) / 16383.0f;
                strips[ch]->setMeterLevels(normL, normR);
            }
            break;
        }

        case Sim::MsgType::VcaFaderUpdate:
        {
            int gid = msg.vcaGroupId;
            juce::String name(msg.trackName);

            // Find existing strip
            int existingIdx = -1;
            for (int i = 0; i < (int)vcaStrips.size(); ++i)
                if (vcaStrips[i]->getGroupId() == gid) { existingIdx = i; break; }

            if (name.isEmpty())
            {
                // Remove strip if name is cleared
                if (existingIdx >= 0)
                {
                    vcaContainer.removeChildComponent(vcaStrips[existingIdx].get());
                    vcaStrips.erase(vcaStrips.begin() + existingIdx);
                    resized();
                }
            }
            else if (existingIdx >= 0)
            {
                // Update existing
                vcaStrips[existingIdx]->setGroupName(name);
                vcaStrips[existingIdx]->setFaderValue(msg.vcaValue);
            }
            else
            {
                // Create new
                auto vca = std::make_unique<SimVcaStrip>(gid, name, uiBus);
                vca->setFaderValue(msg.vcaValue);
                vcaContainer.addAndMakeVisible(*vca);
                vcaStrips.push_back(std::move(vca));
                resized();
            }
            break;
        }

        default:
            break;
    }
}

void MainComponent::updateLcdFromBuffer(int offset, int length)
{
    int startPos = offset;
    int endPos   = offset + length;

    // Each MCU unit contributes 112 chars (2 rows × 56).
    // Global layout: unit0 row1 (0-55), unit0 row2 (56-111),
    //                unit1 row1 (112-167), unit1 row2 (168-223),
    //                unit2 row1 (224-279), unit2 row2 (280-335)
    // But the middleware sends offsets already globalized per unit:
    //   unit0: offsets 0-111 (row1: 0-55, row2: 56-111)
    //   unit1: offsets 112-223 (row1: 112-167, row2: 168-223)
    //   unit2: offsets 224-335 (row1: 224-279, row2: 280-335)
    //
    // Each channel gets 7 chars. Channel i (global 0-23):
    //   unit = i / 8, localCh = i % 8
    //   row1 offset = unit*112 + localCh*7
    //   row2 offset = unit*112 + 56 + localCh*7

    for (int ch = 0; ch < kTotalChannels; ++ch)
    {
        int unit    = ch / 8;
        int localCh = ch % 8;

        int topStart = unit * 112 + localCh * 7;
        int topEnd   = topStart + 7;
        if (startPos < topEnd && endPos > topStart)
        {
            juce::String text(lcdBuffer + topStart, 7);
            strips[ch]->setLcdText(0, text.trimEnd());
        }

        int botStart = unit * 112 + 56 + localCh * 7;
        int botEnd   = botStart + 7;
        if (startPos < botEnd && endPos > botStart)
        {
            juce::String text(lcdBuffer + botStart, 7);
            strips[ch]->setLcdText(1, text.trimEnd());
        }
    }
}
