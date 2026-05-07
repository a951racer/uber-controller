// PluginProcessor.h
// Uber Channel Agent — a utility plugin inserted on DAW channels to report
// metadata to the middleware. No audio processing.
#pragma once
#include <JuceHeader.h>
#include "ChannelReporter.h"

class UberChannelAgentProcessor : public juce::AudioProcessor
{
public:
    UberChannelAgentProcessor();
    ~UberChannelAgentProcessor() override;

    // AudioProcessor overrides (no-op for audio)
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Uber Channel Agent"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Accessors for the editor
    ChannelReporter& getReporter() { return reporter; }
    juce::String getTrackName() const { return currentTrackName; }
    juce::String getTrackType() const { return trackType; }
    juce::String getTrackUuid() const { return trackUuid; }
    int getMcuChannel() const { return mcuChannel; }

    /** Called by the editor to update the track name. */
    void setTrackName(const juce::String& name);

    /** Called by the editor to update the track type. */
    void setTrackType(const juce::String& type);

    /** Called by the editor or externally to set the MCU channel. */
    void setMcuChannel(int ch);

private:
    juce::String generateUuid();

    ChannelReporter reporter;

    juce::String trackUuid;
    juce::String pluginInstanceId;
    juce::String currentTrackName;
    juce::String trackType { "Audio" };
    int          mcuChannel = -1;

    juce::String middlewareHost { "127.0.0.1" };
    int          middlewarePort = 9001;
};
