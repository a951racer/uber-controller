// GroupReporter.h
// TCP client that connects to the middleware and sends group definitions.
// Also receives track assignment updates from the middleware for the matrix display.
#pragma once
#include <JuceHeader.h>
#include <mutex>
#include <atomic>
#include <functional>

struct GroupDef
{
    int          id   = 0;   // 1-8
    juce::String name;
};

struct TrackAssignment
{
    juce::String trackUuid;
    juce::String trackName;
    int          mcuChannel = -1;
    int          groupId    = 0;  // 0 = unassigned

    bool operator==(const TrackAssignment& other) const
    {
        return trackUuid == other.trackUuid && trackName == other.trackName &&
               mcuChannel == other.mcuChannel && groupId == other.groupId;
    }

    bool operator!=(const TrackAssignment& other) const
    {
        return !(*this == other);
    }
};

class GroupReporter : private juce::Thread
{
public:
    using TrackUpdateCallback = std::function<void(const std::vector<TrackAssignment>&)>;

    GroupReporter();
    ~GroupReporter() override;

    void start(const juce::String& host, int port);
    void stop();

    void setGroups(const std::vector<GroupDef>& groups);
    void setTrackUpdateCallback(TrackUpdateCallback cb) { onTrackUpdate = std::move(cb); }

    /** Send transport/session info to the middleware. */
    void sendTransportInfo(double bpm, int timeSigNum, int timeSigDen,
                           double ppqPosition, int64_t timeInSamples,
                           double sampleRate,
                           bool isPlaying, bool isLooping,
                           double loopStartPpq, double loopEndPpq,
                           int barNumber, int beatNumber);

    bool isConnected() const { return connected.load(); }

    static constexpr int kReconnectMs = 5000;

private:
    void run() override;
    bool tryConnect();
    void sendGroupDefs();
    void sendJson(const juce::String& json);
    void processIncoming(const juce::String& line);

    juce::String host;
    int          port = 9001;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { false };

    std::mutex mutex;
    std::vector<GroupDef> currentGroups;
    std::atomic<bool> needsSend { false };

    TrackUpdateCallback onTrackUpdate;
};
