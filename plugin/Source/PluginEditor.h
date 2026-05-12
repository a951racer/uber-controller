// PluginEditor.h
// Shows connection status, auto-detected channel info, and group assignment.
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class UberChannelAgentEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit UberChannelAgentEditor(UberChannelAgentProcessor&);
    ~UberChannelAgentEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshGroupCombo();

    UberChannelAgentProcessor& processor;

    juce::Label titleLabel   { {}, "Uber Channel Agent" };
    juce::Label statusLabel  { {}, "Disconnected" };
    juce::Label pslLabel     { {}, "PSL: unavailable" };

    // Auto-detected info (read-only display)
    juce::Label channelInfoLabel { {}, "" };

    // Group assignment
    juce::Label    groupLabel { {}, "Group:" };
    juce::ComboBox groupCombo;

    std::vector<GroupInfo> lastKnownGroups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UberChannelAgentEditor)
};
