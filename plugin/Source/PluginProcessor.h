// PluginProcessor.h
// Uber Channel Agent — reads/writes DAW mixer state via PSL extensions,
// reports to middleware via TCP.
#pragma once
#include <JuceHeader.h>
#include "ChannelReporter.h"
#include "PslContextBridge.h"
#include "MeterSender.h"
#include "pluginterfaces/base/funknown.h"

class UberChannelAgentProcessor : public juce::AudioProcessor,
                                   public juce::VST3ClientExtensions
{
public:
    UberChannelAgentProcessor();
    ~UberChannelAgentProcessor() override;

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    // VST3ClientExtensions overrides
    void setIComponentHandler(Steinberg::FUnknown* handler) override;
    int32_t queryIEditController(const Steinberg::TUID iid, void** obj) override;

    // Accessors
    ChannelReporter& getReporter() { return reporter; }
    PslContextBridge& getPslBridge() { return pslBridge; }
    ChannelMixerState getMixerState() const { return pslBridge.getState(); }

    juce::String getTrackUuid() const { return trackUuid; }
    int getGroupId() const { return groupId; }

    std::vector<GroupInfo> getAvailableGroups() const;

    // Setters (for values not auto-detected by PSL)
    void setGroupId(int id);

    // Write mixer state to DAW (called by middleware commands)
    void setVolume(double normValue);
    void setPan(double value);
    void setMute(bool muted);
    void setSolo(bool soloed);
    void setSelected(bool selected);

private:
    juce::String generateUuid();
    void onMixerStateChanged(const ChannelMixerState& state);
    void sendFullState();

    ChannelReporter  reporter;
    PslContextBridge pslBridge;
    MeterSender      meterSender;

    juce::String trackUuid;
    juce::String pluginInstanceId;
    int          groupId = 0;

    mutable std::mutex groupsMutex;
    std::vector<GroupInfo> availableGroups;

    juce::String middlewareHost { "127.0.0.1" };
    int          middlewarePort = 9001;

    int meterBlockCounter = 0;
    static constexpr int kMeterSendInterval = 3;  // send every N processBlock calls
};
