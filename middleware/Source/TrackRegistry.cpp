// TrackRegistry.cpp
#include "TrackRegistry.h"

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
                existing.mcuChannel != info.mcuChannel)
            {
                existing.name       = info.name;
                existing.type       = info.type;
                existing.mcuChannel = info.mcuChannel;
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
