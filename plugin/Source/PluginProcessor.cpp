// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

UberChannelAgentProcessor::UberChannelAgentProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Generate stable IDs
    pluginInstanceId = generateUuid();
    trackUuid        = generateUuid();

    // Initialize reporter with current info before starting
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel);

    // Start reporter
    reporter.start(middlewareHost, middlewarePort);
}

UberChannelAgentProcessor::~UberChannelAgentProcessor()
{
    reporter.stop();
}

void UberChannelAgentProcessor::setMcuChannel(int ch)
{
    mcuChannel = ch;
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel);
}

void UberChannelAgentProcessor::setTrackName(const juce::String& name)
{
    currentTrackName = name;
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel);
}

void UberChannelAgentProcessor::setTrackType(const juce::String& type)
{
    trackType = type;
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel);
}

juce::String UberChannelAgentProcessor::generateUuid()
{
    return juce::Uuid().toString();
}

void UberChannelAgentProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstanceId);
    obj->setProperty("track_name", currentTrackName);
    obj->setProperty("track_type", trackType);
    obj->setProperty("mcu_channel", mcuChannel);
    obj->setProperty("middleware_host", middlewareHost);
    obj->setProperty("middleware_port", middlewarePort);

    juce::String json = juce::JSON::toString(juce::var(obj));
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void UberChannelAgentProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::String json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    auto parsed = juce::JSON::parse(json);

    if (auto* obj = parsed.getDynamicObject())
    {
        if (obj->hasProperty("track_uuid"))
            trackUuid = obj->getProperty("track_uuid").toString();
        if (obj->hasProperty("plugin_instance"))
            pluginInstanceId = obj->getProperty("plugin_instance").toString();
        if (obj->hasProperty("track_name"))
            currentTrackName = obj->getProperty("track_name").toString();
        if (obj->hasProperty("track_type"))
            trackType = obj->getProperty("track_type").toString();
        if (obj->hasProperty("mcu_channel"))
            mcuChannel = static_cast<int>(obj->getProperty("mcu_channel"));

        juce::String newHost = middlewareHost;
        int newPort = middlewarePort;

        if (obj->hasProperty("middleware_host"))
            newHost = obj->getProperty("middleware_host").toString();
        if (obj->hasProperty("middleware_port"))
            newPort = static_cast<int>(obj->getProperty("middleware_port"));

        // Only restart reporter if host/port changed
        if (newHost != middlewareHost || newPort != middlewarePort)
        {
            middlewareHost = newHost;
            middlewarePort = newPort;
            reporter.stop();
            reporter.start(middlewareHost, middlewarePort);
        }
    }

    // Update reporter with restored state (only sends if values differ from what was set in constructor)
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel);
}

juce::AudioProcessorEditor* UberChannelAgentProcessor::createEditor()
{
    return new UberChannelAgentEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UberChannelAgentProcessor();
}
