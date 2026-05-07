// PluginEditor.h
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

    juce::Label titleLabel       { {}, "Uber Channel Agent" };
    juce::Label statusLabel      { {}, "Disconnected" };
    juce::Label uuidLabel        { {}, "" };

    juce::Label trackNameLabel   { {}, "Track Name:" };
    juce::TextEditor trackNameEditor;

    juce::Label trackTypeLabel   { {}, "Type:" };
    juce::ComboBox trackTypeCombo;

    juce::Label mcuChannelLabel  { {}, "MCU Channel:" };
    juce::ComboBox mcuChannelCombo;

    juce::Label groupLabel       { {}, "Group:" };
    juce::ComboBox groupCombo;

    std::vector<GroupInfo> lastKnownGroups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UberChannelAgentEditor)
};
