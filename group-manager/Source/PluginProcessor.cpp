// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

UberGroupManagerProcessor::UberGroupManagerProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Default: 8 groups, unnamed
    for (int i = 1; i <= 8; ++i)
        groups.push_back({ i, "" });

    reporter.setTrackUpdateCallback([this](const std::vector<TrackAssignment>& assignments)
    {
        std::lock_guard<std::mutex> lock(trackMutex);
        trackAssignments = assignments;
    });

    reporter.setGroups(groups);
    reporter.start(middlewareHost, middlewarePort);
}

UberGroupManagerProcessor::~UberGroupManagerProcessor()
{
    reporter.stop();
}

void UberGroupManagerProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    if (++transportBlockCounter < kTransportSendInterval)
        return;
    transportBlockCounter = 0;

    auto* playHead = getPlayHead();
    if (!playHead) return;

    auto posInfo = playHead->getPosition();
    if (!posInfo.hasValue()) return;

    double bpm = posInfo->getBpm().orFallback(120.0);

    int timeSigN = 4, timeSigD = 4;
    if (auto ts = posInfo->getTimeSignature())
    {
        timeSigN = ts->numerator;
        timeSigD = ts->denominator;
    }

    double ppq = posInfo->getPpqPosition().orFallback(0.0);
    int64_t samples = posInfo->getTimeInSamples().orFallback(0);
    bool playing = posInfo->getIsPlaying();
    bool looping = posInfo->getIsLooping();

    double loopStart = 0.0, loopEnd = 0.0;
    if (auto loopPoints = posInfo->getLoopPoints())
    {
        loopStart = loopPoints->ppqStart;
        loopEnd   = loopPoints->ppqEnd;
    }

    double beatsPerBar = timeSigN;
    int barNumber = (int)(ppq / beatsPerBar) + 1;
    int beatNumber = (int)(std::fmod(ppq, beatsPerBar)) + 1;

    reporter.sendTransportInfo(bpm, timeSigN, timeSigD,
                               ppq, samples, getSampleRate(), playing, looping,
                               loopStart, loopEnd, barNumber, beatNumber);
}

std::vector<GroupDef> UberGroupManagerProcessor::getGroups() const
{
    std::lock_guard<std::mutex> lock(groupMutex);
    return groups;
}

void UberGroupManagerProcessor::setGroupName(int groupId, const juce::String& name)
{
    {
        std::lock_guard<std::mutex> lock(groupMutex);
        for (auto& g : groups)
        {
            if (g.id == groupId)
            {
                if (g.name == name) return;
                g.name = name;
                break;
            }
        }
    }
    reporter.setGroups(getGroups());
}

void UberGroupManagerProcessor::setGroupCount(int count)
{
    count = juce::jlimit(1, 8, count);
    {
        std::lock_guard<std::mutex> lock(groupMutex);
        groups.resize(count);
        for (int i = 0; i < count; ++i)
            if (groups[i].id == 0)
                groups[i].id = i + 1;
    }
    reporter.setGroups(getGroups());
}

std::vector<TrackAssignment> UberGroupManagerProcessor::getTrackAssignments() const
{
    std::lock_guard<std::mutex> lock(trackMutex);
    return trackAssignments;
}

void UberGroupManagerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto* obj = new juce::DynamicObject();

    juce::Array<juce::var> groupArray;
    {
        std::lock_guard<std::mutex> lock(groupMutex);
        for (auto& g : groups)
        {
            auto* gObj = new juce::DynamicObject();
            gObj->setProperty("id", g.id);
            gObj->setProperty("name", g.name);
            groupArray.add(juce::var(gObj));
        }
    }
    obj->setProperty("groups", groupArray);
    obj->setProperty("middleware_host", middlewareHost);
    obj->setProperty("middleware_port", middlewarePort);

    juce::String json = juce::JSON::toString(juce::var(obj));
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void UberGroupManagerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::String json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    auto parsed = juce::JSON::parse(json);
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    if (obj->hasProperty("groups"))
    {
        auto* arr = obj->getProperty("groups").getArray();
        if (arr)
        {
            std::lock_guard<std::mutex> lock(groupMutex);
            groups.clear();
            for (auto& item : *arr)
            {
                auto* gObj = item.getDynamicObject();
                if (gObj)
                {
                    GroupDef g;
                    g.id   = static_cast<int>(gObj->getProperty("id"));
                    g.name = gObj->getProperty("name").toString();
                    groups.push_back(g);
                }
            }
        }
    }

    if (obj->hasProperty("middleware_host"))
        middlewareHost = obj->getProperty("middleware_host").toString();
    if (obj->hasProperty("middleware_port"))
        middlewarePort = static_cast<int>(obj->getProperty("middleware_port"));

    reporter.setGroups(getGroups());
}

juce::AudioProcessorEditor* UberGroupManagerProcessor::createEditor()
{
    return new UberGroupManagerEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UberGroupManagerProcessor();
}
