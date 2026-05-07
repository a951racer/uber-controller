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

    // Accessors
    ChannelReporter& getReporter() { return reporter; }
    juce::String getTrackName() const { return currentTrackName; }
    juce::String getTrackType() const { return trackType; }
    juce::String getTrackUuid() const { return trackUuid; }
    int getMcuChannel() const { return mcuChannel; }
    int getGroupId() const { return groupId; }

    // Available groups (received from middleware)
    std::vector<GroupInfo> getAvailableGroups() const;

    // Setters (called by editor)
    void setTrackName(const juce::String& name);
    void setTrackType(const juce::String& type);
    void setMcuChannel(int ch);
    void setGroupId(int id);

private:
    juce::String generateUuid();
    void sendUpdate();

    ChannelReporter reporter;

    juce::String trackUuid;
    juce::String pluginInstanceId;
    juce::String currentTrackName;
    juce::String trackType { "Audio" };
    int          mcuChannel = -1;
    int          groupId    = 0;  // 0 = no group

    // Groups received from middleware
    mutable std::mutex groupsMutex;
    std::vector<GroupInfo> availableGroups;

    juce::String middlewareHost { "127.0.0.1" };
    int          middlewarePort = 9001;
};
