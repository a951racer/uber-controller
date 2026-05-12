// TrackRegistry.cpp
#include "TrackRegistry.h"
#include <cmath>

TrackRegistry::TrackRegistry() {}

bool TrackRegistry::registerTrack(const TrackInfo& info)
{
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = tracks.find(info.trackUuid);

        if (it == tracks.end())
        {
            tracks[info.trackUuid] = info;
            tracks[info.trackUuid].lastSeen = juce::Time::getMillisecondCounterHiRes();
            changed = true;
        }
        else
        {
            auto& existing = it->second;
            if (existing.name != info.name ||
                existing.type != info.type ||
                existing.mcuChannel != info.mcuChannel ||
                existing.groupId != info.groupId ||
                std::abs(existing.volume - info.volume) > 0.001 ||
                std::abs(existing.pan - info.pan) > 0.005 ||
                existing.mute != info.mute ||
                existing.solo != info.solo ||
                existing.selected != info.selected)
            {
                existing.name       = info.name;
                existing.type       = info.type;
                existing.mcuChannel = info.mcuChannel;
                existing.groupId    = info.groupId;
                existing.volume     = info.volume;
                existing.pan        = info.pan;
                existing.mute       = info.mute;
                existing.solo       = info.solo;
                existing.selected   = info.selected;
                existing.lastSeen   = juce::Time::getMillisecondCounterHiRes();
                changed = true;
            }
            else
            {
                existing.lastSeen = juce::Time::getMillisecondCounterHiRes();
            }
        }
    } // mutex released here

    if (changed && onChange)
        onChange();

    return changed;
}

void TrackRegistry::heartbeat(const juce::String& trackUuid,
                              const juce::String& pluginInstance,
                              int mcuChannel)
{
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = tracks.find(trackUuid);
        if (it != tracks.end())
        {
            it->second.lastSeen = juce::Time::getMillisecondCounterHiRes();
            if (it->second.mcuChannel != mcuChannel)
            {
                it->second.mcuChannel = mcuChannel;
                changed = true;
            }
        }
    }

    if (changed && onChange)
        onChange();
}

void TrackRegistry::removeTrack(const juce::String& trackUuid)
{
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = tracks.find(trackUuid);
        if (it != tracks.end())
        {
            tracks.erase(it);
            changed = true;
        }
    }

    if (changed && onChange)
        onChange();
}

void TrackRegistry::pruneStale(double timeoutMs)
{
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex);
        double now = juce::Time::getMillisecondCounterHiRes();

        for (auto it = tracks.begin(); it != tracks.end(); )
        {
            if (now - it->second.lastSeen > timeoutMs)
            {
                it = tracks.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
    }

    if (changed && onChange)
        onChange();
}

std::vector<TrackInfo> TrackRegistry::getAllTracks() const
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<TrackInfo> result;
    result.reserve(tracks.size());
    for (auto& [uuid, info] : tracks)
        result.push_back(info);
    return result;
}

const TrackInfo* TrackRegistry::getTrackForMcuChannel(int mcuChannel) const
{
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& [uuid, info] : tracks)
        if (info.mcuChannel == mcuChannel)
            return &info;
    return nullptr;
}

void TrackRegistry::setGroups(const std::vector<GroupDef>& groups)
{
    {
        std::lock_guard<std::mutex> lock(groupMutex);
        groupDefs = groups;
    }

    if (onChange)
        onChange();
}

std::vector<TrackRegistry::GroupDef> TrackRegistry::getGroups() const
{
    std::lock_guard<std::mutex> lock(groupMutex);
    return groupDefs;
}

bool TrackRegistry::updateMixerState(const juce::String& trackUuid, double volume, double pan,
                                     bool mute, bool solo, bool selected)
{
    std::lock_guard<std::mutex> lock(mutex);

    auto it = tracks.find(trackUuid);
    if (it == tracks.end()) return false;

    auto& t = it->second;
    bool changed = false;

    if (std::abs(t.volume - volume) > 0.001) { t.volume = volume; changed = true; }
    if (std::abs(t.pan - pan) > 0.005)       { t.pan = pan; changed = true; }
    if (t.mute != mute)                       { t.mute = mute; changed = true; }
    if (t.solo != solo)                       { t.solo = solo; changed = true; }
    if (t.selected != selected)               { t.selected = selected; changed = true; }

    return changed;
}
