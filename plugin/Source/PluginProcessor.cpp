// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

UberChannelAgentProcessor::UberChannelAgentProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    pluginInstanceId = generateUuid();
    trackUuid        = generateUuid();

    // PSL state change callback — fires when the DAW changes volume/pan/mute/etc.
    pslBridge.setStateChangeCallback([this](const ChannelMixerState& state)
    {
        onMixerStateChanged(state);
    });

    // Group list callback from middleware
    reporter.setGroupListCallback([this](const std::vector<GroupInfo>& groups)
    {
        std::lock_guard<std::mutex> lock(groupsMutex);
        availableGroups = groups;
    });

    // Mixer commands from middleware (simulator moved a fader, pressed mute, etc.)
    reporter.setMixerCmdCallback([this](const MixerCommand& cmd)
    {
        switch (cmd.type)
        {
            case MixerCommand::SetVolume:  setVolume(cmd.value);    break;
            case MixerCommand::SetPan:     setPan(cmd.value);       break;
            case MixerCommand::SetMute:    setMute(cmd.flag);       break;
            case MixerCommand::SetSolo:    setSolo(cmd.flag);       break;
            case MixerCommand::SetSelect:  setSelected(cmd.flag);   break;
        }
    });

    reporter.start(middlewareHost, middlewarePort);
    meterSender.start(middlewareHost, 9002);
}

UberChannelAgentProcessor::~UberChannelAgentProcessor()
{
    meterSender.stop();
    reporter.stop();
}

void UberChannelAgentProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Measure peak levels
    float peakL = 0.0f, peakR = 0.0f;
    int numChannels = buffer.getNumChannels();
    int numSamples  = buffer.getNumSamples();

    if (numChannels > 0)
        peakL = buffer.getMagnitude(0, 0, numSamples);
    if (numChannels > 1)
        peakR = buffer.getMagnitude(1, 0, numSamples);
    else
        peakR = peakL;  // mono → same on both

    // Throttle meter sends (~30Hz at 44.1kHz/128 samples = every 3 blocks)
    if (++meterBlockCounter >= kMeterSendInterval)
    {
        meterBlockCounter = 0;
        auto state = pslBridge.getState();
        if (state.channelIndex >= 0)
            meterSender.sendMeter(state.channelIndex, peakL, peakR);
    }
}

// ---------------------------------------------------------------------------
// VST3ClientExtensions
// ---------------------------------------------------------------------------

void UberChannelAgentProcessor::setIComponentHandler(Steinberg::FUnknown* handler)
{
    pslBridge.setComponentHandler(handler);

    if (pslBridge.isAvailable())
        sendFullState();
}

int32_t UberChannelAgentProcessor::queryIEditController(const Steinberg::TUID iid, void** obj)
{
    auto* handlerInterface = pslBridge.getHandlerInterface();
    if (handlerInterface)
    {
        auto result = handlerInterface->queryInterface(iid, obj);
        if (result == Steinberg::kResultOk)
            return result;
    }
    *obj = nullptr;
    return Steinberg::kNoInterface;
}

// ---------------------------------------------------------------------------
// Mixer state changes from DAW → middleware
// ---------------------------------------------------------------------------

void UberChannelAgentProcessor::onMixerStateChanged(const ChannelMixerState& mixState)
{
    juce::String typeStr;
    switch (mixState.channelType)
    {
        case Presonus::ContextInfo::kTrack: typeStr = "Audio"; break;
        case Presonus::ContextInfo::kBus:   typeStr = "Bus"; break;
        case Presonus::ContextInfo::kFX:    typeStr = "FX"; break;
        case Presonus::ContextInfo::kSynth: typeStr = "Instrument"; break;
        case Presonus::ContextInfo::kIn:    typeStr = "Input"; break;
        case Presonus::ContextInfo::kOut:   typeStr = "Master"; break;
        default: typeStr = "Audio"; break;
    }

    // Send raw PSL volume (0.0 = silence, 1.0 = 0dB, >1.0 = positive dB)
    // Also send maxVolume so middleware knows the range
    auto state = pslBridge.getState();
    // Normalize volume: PSL linear gain → fader position (0-1) using dB-linear curve
    // Same curve as setVolume but reversed
    static constexpr double kMinDB = -70.0;
    static constexpr double kMaxDB = 10.0;
    static constexpr double kRange = kMaxDB - kMinDB;

    double normVolume;
    if (mixState.volume <= 0.00001)
    {
        normVolume = 0.0;
    }
    else
    {
        double dB = 20.0 * std::log10(mixState.volume);
        if (dB < kMinDB) dB = kMinDB;
        if (dB > kMaxDB) dB = kMaxDB;
        normVolume = (dB - kMinDB) / kRange;
    }

    reporter.setChannelState(trackUuid, pluginInstanceId,
                             mixState.channelName, typeStr,
                             mixState.channelIndex, groupId,
                             normVolume, 3.162, mixState.pan,
                             mixState.mute, mixState.solo,
                             mixState.selected);
}

void UberChannelAgentProcessor::sendFullState()
{
    auto mixState = pslBridge.getState();
    onMixerStateChanged(mixState);
}

// ---------------------------------------------------------------------------
// Write to DAW (commands from middleware/simulator)
// ---------------------------------------------------------------------------

void UberChannelAgentProcessor::setVolume(double normValue)
{
    // Convert fader position (0-1) to PSL volume using a dB-linear curve
    // Fader 0.0 = -inf, 0.75 = 0dB (unity), 1.0 = +10dB
    // Map: fader 0-1 → dB range -70 to +10 (linearly in dB)
    // Then convert dB to linear gain
    static constexpr double kMinDB = -70.0;
    static constexpr double kMaxDB = 10.0;
    static constexpr double kRange = kMaxDB - kMinDB;  // 80 dB

    double pslVolume;
    if (normValue <= 0.001)
    {
        pslVolume = 0.0;  // silence
    }
    else
    {
        double dB = kMinDB + normValue * kRange;  // linear in dB
        pslVolume = std::pow(10.0, dB / 20.0);
    }

    pslBridge.setVolume(pslVolume);
}

void UberChannelAgentProcessor::setPan(double value)
{
    pslBridge.setPan(value);
}

void UberChannelAgentProcessor::setMute(bool muted)
{
    pslBridge.setMute(muted);
}

void UberChannelAgentProcessor::setSolo(bool soloed)
{
    pslBridge.setSolo(soloed);
}

void UberChannelAgentProcessor::setSelected(bool selected)
{
    pslBridge.setSelected(selected);
}

void UberChannelAgentProcessor::setGroupId(int id)
{
    groupId = id;
    sendFullState();
}

std::vector<GroupInfo> UberChannelAgentProcessor::getAvailableGroups() const
{
    std::lock_guard<std::mutex> lock(groupsMutex);
    return availableGroups;
}

juce::String UberChannelAgentProcessor::generateUuid()
{
    return juce::Uuid().toString();
}

// ---------------------------------------------------------------------------
// State persistence
// ---------------------------------------------------------------------------

void UberChannelAgentProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstanceId);
    obj->setProperty("group_id", groupId);
    obj->setProperty("middleware_host", middlewareHost);
    obj->setProperty("middleware_port", middlewarePort);

    juce::String json = juce::JSON::toString(juce::var(obj));
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void UberChannelAgentProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::String json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    auto parsed = juce::JSON::parse(json);
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    if (obj->hasProperty("track_uuid"))
        trackUuid = obj->getProperty("track_uuid").toString();
    if (obj->hasProperty("plugin_instance"))
        pluginInstanceId = obj->getProperty("plugin_instance").toString();
    if (obj->hasProperty("group_id"))
        groupId = static_cast<int>(obj->getProperty("group_id"));

    juce::String newHost = middlewareHost;
    int newPort = middlewarePort;
    if (obj->hasProperty("middleware_host"))
        newHost = obj->getProperty("middleware_host").toString();
    if (obj->hasProperty("middleware_port"))
        newPort = static_cast<int>(obj->getProperty("middleware_port"));

    if (newHost != middlewareHost || newPort != middlewarePort)
    {
        middlewareHost = newHost;
        middlewarePort = newPort;
        reporter.stop();
        reporter.start(middlewareHost, middlewarePort);
    }

    // PSL will provide the rest (name, type, index, volume, etc.)
    if (pslBridge.isAvailable())
        sendFullState();
}

juce::AudioProcessorEditor* UberChannelAgentProcessor::createEditor()
{
    return new UberChannelAgentEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UberChannelAgentProcessor();
}
