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
    int          mcuChannel = -1;
    int          groupId    = 0;
    double       volume     = 1.0;
    double       maxVolume  = 3.162;  // PSL max volume (linear gain at +10dB)
    double       pan        = 0.5;
    bool         mute       = false;
    bool         solo       = false;
    bool         selected   = false;
    double       lastSeen   = 0.0;
};

class TrackRegistry
{
public:
    using ChangeCallback = std::function<void()>;

    TrackRegistry();

    /** Register or update a track. Returns true if something changed.
        Fires onChange callback if metadata (name/type/group/channel) changed. */
    bool registerTrack(const TrackInfo& info);

    /** Update only mixer state (volume/pan/mute/solo/selected) without firing onChange.
        Returns true if something changed. Used for high-frequency updates from simulator. */
    bool updateMixerState(const juce::String& trackUuid, double volume, double pan,
                          bool mute, bool solo, bool selected);

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

    // Group definitions (set by Group Manager plugin)
    struct GroupDef { int id = 0; juce::String name; };

    void setGroups(const std::vector<GroupDef>& groups);
    std::vector<GroupDef> getGroups() const;

private:
    mutable std::mutex mutex;
    std::map<juce::String, TrackInfo> tracks;  // keyed by track_uuid

    mutable std::mutex groupMutex;
    std::vector<GroupDef> groupDefs;

    ChangeCallback onChange;
};
