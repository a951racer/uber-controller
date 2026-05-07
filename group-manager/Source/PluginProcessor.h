// PluginProcessor.h
// Uber Group Manager — defines up to 8 VCA/groups.
// Single instance per song. Communicates group definitions to the middleware.
#pragma once
#include <JuceHeader.h>
#include "GroupReporter.h"

class UberGroupManagerProcessor : public juce::AudioProcessor
{
public:
    UberGroupManagerProcessor();
    ~UberGroupManagerProcessor() override;

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Uber Group Manager"; }
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

    // Group management
    std::vector<GroupDef> getGroups() const;
    void setGroupName(int groupId, const juce::String& name);
    void setGroupCount(int count);

    // Track assignments (received from middleware for matrix display)
    std::vector<TrackAssignment> getTrackAssignments() const;

    GroupReporter& getReporter() { return reporter; }

private:
    GroupReporter reporter;

    mutable std::mutex groupMutex;
    std::vector<GroupDef> groups;

    mutable std::mutex trackMutex;
    std::vector<TrackAssignment> trackAssignments;

    juce::String middlewareHost { "127.0.0.1" };
    int          middlewarePort = 9001;
};
