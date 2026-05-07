// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

UberChannelAgentProcessor::UberChannelAgentProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    pluginInstanceId = generateUuid();
    trackUuid        = generateUuid();

    reporter.setGroupListCallback([this](const std::vector<GroupInfo>& groups)
    {
        std::lock_guard<std::mutex> lock(groupsMutex);
        availableGroups = groups;
    });

    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel, groupId);
    reporter.start(middlewareHost, middlewarePort);
}

UberChannelAgentProcessor::~UberChannelAgentProcessor()
{
    reporter.stop();
}

void UberChannelAgentProcessor::sendUpdate()
{
    reporter.setTrackInfo(trackUuid, pluginInstanceId,
                          currentTrackName, trackType, mcuChannel, groupId);
}

void UberChannelAgentProcessor::setTrackName(const juce::String& name)
{
    currentTrackName = name;
    sendUpdate();
}

void UberChannelAgentProcessor::setTrackType(const juce::String& type)
{
    trackType = type;
    sendUpdate();
}

void UberChannelAgentProcessor::setMcuChannel(int ch)
{
    mcuChannel = ch;
    sendUpdate();
}

void UberChannelAgentProcessor::setGroupId(int id)
{
    groupId = id;
    sendUpdate();
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

void UberChannelAgentProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstanceId);
    obj->setProperty("track_name", currentTrackName);
    obj->setProperty("track_type", trackType);
    obj->setProperty("mcu_channel", mcuChannel);
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
    if (obj->hasProperty("track_name"))
        currentTrackName = obj->getProperty("track_name").toString();
    if (obj->hasProperty("track_type"))
        trackType = obj->getProperty("track_type").toString();
    if (obj->hasProperty("mcu_channel"))
        mcuChannel = static_cast<int>(obj->getProperty("mcu_channel"));
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

    sendUpdate();
}

juce::AudioProcessorEditor* UberChannelAgentProcessor::createEditor()
{
    return new UberChannelAgentEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UberChannelAgentProcessor();
}
