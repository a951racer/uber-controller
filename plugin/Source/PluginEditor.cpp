// PluginEditor.cpp
#include "PluginEditor.h"

UberChannelAgentEditor::UberChannelAgentEditor(UberChannelAgentProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(11.0f));

    addAndMakeVisible(pslLabel);
    pslLabel.setFont(juce::Font(11.0f));

    addAndMakeVisible(channelInfoLabel);
    channelInfoLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    channelInfoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    // Group
    addAndMakeVisible(groupLabel);
    addAndMakeVisible(groupCombo);
    groupCombo.addItem("(none)", 1);
    groupCombo.setSelectedId(1, juce::dontSendNotification);
    groupCombo.onChange = [this]
    {
        int sel = groupCombo.getSelectedId();
        int gid = (sel <= 1) ? 0 : sel - 1;
        processor.setGroupId(gid);
    };

    startTimerHz(4);
    setSize(320, 220);
}

UberChannelAgentEditor::~UberChannelAgentEditor()
{
    stopTimer();
}

void UberChannelAgentEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void UberChannelAgentEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    titleLabel     .setBounds(bounds.removeFromTop(24));
    statusLabel    .setBounds(bounds.removeFromTop(16));
    pslLabel       .setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(6);
    channelInfoLabel.setBounds(bounds.removeFromTop(60));
    bounds.removeFromTop(6);

    auto groupRow = bounds.removeFromTop(24);
    groupLabel.setBounds(groupRow.removeFromLeft(60));
    groupCombo.setBounds(groupRow.removeFromLeft(150));
}

void UberChannelAgentEditor::timerCallback()
{
    // Connection status
    if (processor.getReporter().isConnected())
    {
        statusLabel.setText("Middleware: Connected", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        statusLabel.setText("Middleware: Disconnected", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }

    // PSL status
    if (processor.getPslBridge().isAvailable())
    {
        pslLabel.setText("PSL: active", juce::dontSendNotification);
        pslLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        pslLabel.setText("PSL: unavailable (host doesn't support it)", juce::dontSendNotification);
        pslLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }

    // Channel info from PSL
    auto state = processor.getMixerState();
    juce::String info;
    info << "Name:    " << (state.channelName.isEmpty() ? "(unknown)" : state.channelName) << "\n";
    info << "Index:   " << (state.channelIndex >= 0 ? juce::String(state.channelIndex + 1) : "-") << "\n";
    info << "Volume:  " << juce::String(state.volume, 3) << "  Pan: " << juce::String(state.pan, 2) << "\n";
    info << "Mute:    " << (state.mute ? "ON" : "off") << "  Solo: " << (state.solo ? "ON" : "off");
    channelInfoLabel.setText(info, juce::dontSendNotification);

    refreshGroupCombo();
}

void UberChannelAgentEditor::refreshGroupCombo()
{
    auto groups = processor.getAvailableGroups();
    if (groups == lastKnownGroups) return;
    lastKnownGroups = groups;

    int currentGroupId = processor.getGroupId();
    groupCombo.clear(juce::dontSendNotification);
    groupCombo.addItem("(none)", 1);

    for (auto& g : groups)
    {
        juce::String label = g.name.isEmpty()
                                 ? ("Group " + juce::String(g.id))
                                 : g.name;
        groupCombo.addItem(label, g.id + 1);
    }

    if (currentGroupId > 0)
        groupCombo.setSelectedId(currentGroupId + 1, juce::dontSendNotification);
    else
        groupCombo.setSelectedId(1, juce::dontSendNotification);
}
