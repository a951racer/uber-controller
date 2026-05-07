// PluginEditor.h
// Simple UI showing connection status and allowing track name / MCU channel config.
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UberChannelAgentEditor)
};
