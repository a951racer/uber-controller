// PluginEditor.cpp
#include "PluginEditor.h"

static const juce::StringArray kTrackTypes = { "Audio", "Instrument", "Bus", "FX", "VCA", "Master" };

UberChannelAgentEditor::UberChannelAgentEditor(UberChannelAgentProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(uuidLabel);
    uuidLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
    uuidLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    // Track name
    addAndMakeVisible(trackNameLabel);
    addAndMakeVisible(trackNameEditor);
    trackNameEditor.setText(processor.getTrackName(), false);
    trackNameEditor.onReturnKey = [this]
    {
        juce::String newName = trackNameEditor.getText();
        if (newName != processor.getTrackName())
            processor.setTrackName(newName);
    };
    trackNameEditor.onFocusLost = [this]
    {
        juce::String newName = trackNameEditor.getText();
        if (newName != processor.getTrackName())
            processor.setTrackName(newName);
    };

    // Track type
    addAndMakeVisible(trackTypeLabel);
    addAndMakeVisible(trackTypeCombo);
    for (int i = 0; i < kTrackTypes.size(); ++i)
        trackTypeCombo.addItem(kTrackTypes[i], i + 1);
    int typeIdx = kTrackTypes.indexOf(processor.getTrackType());
    trackTypeCombo.setSelectedId(typeIdx >= 0 ? typeIdx + 1 : 1, juce::dontSendNotification);
    trackTypeCombo.onChange = [this]
    {
        juce::String newType = trackTypeCombo.getText();
        if (newType != processor.getTrackType())
            processor.setTrackType(newType);
    };

    // MCU Channel
    addAndMakeVisible(mcuChannelLabel);
    addAndMakeVisible(mcuChannelCombo);
    mcuChannelCombo.addItem("Auto / None", 1);
    for (int i = 0; i < 24; ++i)
        mcuChannelCombo.addItem(juce::String("Ch ") + juce::String(i + 1), i + 2);
    int currentCh = processor.getMcuChannel();
    mcuChannelCombo.setSelectedId(currentCh >= 0 ? currentCh + 2 : 1, juce::dontSendNotification);
    mcuChannelCombo.onChange = [this]
    {
        int sel = mcuChannelCombo.getSelectedId();
        int ch  = (sel <= 1) ? -1 : sel - 2;
        processor.setMcuChannel(ch);
    };

    // Group
    addAndMakeVisible(groupLabel);
    addAndMakeVisible(groupCombo);
    groupCombo.addItem("(none)", 1);
    groupCombo.setSelectedId(1, juce::dontSendNotification);
    groupCombo.onChange = [this]
    {
        int sel = groupCombo.getSelectedId();
        int gid = (sel <= 1) ? 0 : sel - 1;  // item ID 2 = group ID 1, etc.
        processor.setGroupId(gid);
    };

    startTimerHz(2);
    setSize(300, 240);
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

    titleLabel .setBounds(bounds.removeFromTop(24));
    statusLabel.setBounds(bounds.removeFromTop(18));
    uuidLabel  .setBounds(bounds.removeFromTop(14));
    bounds.removeFromTop(8);

    auto nameRow = bounds.removeFromTop(24);
    trackNameLabel.setBounds(nameRow.removeFromLeft(80));
    trackNameEditor.setBounds(nameRow);
    bounds.removeFromTop(6);

    auto typeRow = bounds.removeFromTop(24);
    trackTypeLabel.setBounds(typeRow.removeFromLeft(80));
    trackTypeCombo.setBounds(typeRow.removeFromLeft(120));
    bounds.removeFromTop(6);

    auto chRow = bounds.removeFromTop(24);
    mcuChannelLabel.setBounds(chRow.removeFromLeft(80));
    mcuChannelCombo.setBounds(chRow.removeFromLeft(100));
    bounds.removeFromTop(6);

    auto groupRow = bounds.removeFromTop(24);
    groupLabel.setBounds(groupRow.removeFromLeft(80));
    groupCombo.setBounds(groupRow.removeFromLeft(150));
}

void UberChannelAgentEditor::timerCallback()
{
    if (processor.getReporter().isConnected())
    {
        statusLabel.setText("Connected to middleware", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        statusLabel.setText("Disconnected - retrying...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }

    uuidLabel.setText("UUID: " + processor.getTrackUuid(), juce::dontSendNotification);

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
        groupCombo.addItem(label, g.id + 1);  // item ID = group ID + 1
    }

    // Restore selection
    if (currentGroupId > 0)
        groupCombo.setSelectedId(currentGroupId + 1, juce::dontSendNotification);
    else
        groupCombo.setSelectedId(1, juce::dontSendNotification);
}
