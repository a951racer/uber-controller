// TrackRegistry.h
// Stores track metadata reported by channel agent plugins.
// Correlates plugin-reported MCU channel positions with the middleware's
// channel mapping.
#pragma once
#include <JuceHeader.h>
#include <map>
#include <mutex>
#include <functional>

struct TrackInfo
{
    juce::String trackUuid;
    juce::String pluginInstance;
    juce::String name;
    juce::String type;
    int          mcuChannel = -1;   // 0-based MCU fader position reported by plugin
    double       lastSeen   = 0.0;  // timestamp of last heartbeat/update
};

class TrackRegistry
{
public:
    using ChangeCallback = std::function<void()>;

    TrackRegistry();

    /** Register or update a track. Returns true if something changed. */
    bool registerTrack(const TrackInfo& info);

    /** Mark a track as seen (heartbeat). */
    void heartbeat(const juce::String& trackUuid, const juce::String& pluginInstance, int mcuChannel);

    /** Remove stale tracks (no heartbeat for > timeoutMs). */
    void pruneStale(double timeoutMs = 90000.0);

    /** Remove a specific track (e.g., plugin disconnected). */
    void removeTrack(const juce::String& trackUuid);

    /** Get all registered tracks. */
    std::vector<TrackInfo> getAllTracks() const;

    /** Get track info for a specific MCU channel, or nullptr if none. */
    const TrackInfo* getTrackForMcuChannel(int mcuChannel) const;

    /** Set a callback for when the registry changes. */
    void setChangeCallback(ChangeCallback cb) { onChange = std::move(cb); }

private:
    mutable std::mutex mutex;
    std::map<juce::String, TrackInfo> tracks;  // keyed by track_uuid
    ChangeCallback onChange;
};
