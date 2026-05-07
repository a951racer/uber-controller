// ChannelReporter.h
// Manages a TCP connection to the middleware and sends channel metadata.
// Also receives group list from the middleware (bidirectional).
#pragma once
#include <JuceHeader.h>
#include <functional>
#include <mutex>
#include <atomic>
#include <vector>

struct GroupInfo
{
    int          id = 0;
    juce::String name;

    bool operator==(const GroupInfo& other) const { return id == other.id && name == other.name; }
    bool operator!=(const GroupInfo& other) const { return !(*this == other); }
};

class ChannelReporter : private juce::Thread
{
public:
    using GroupListCallback = std::function<void(const std::vector<GroupInfo>&)>;

    ChannelReporter();
    ~ChannelReporter() override;

    void start(const juce::String& host, int port);
    void stop();

    /** Update the metadata to report. Thread-safe.
        Only triggers a send if values actually changed. */
    void setTrackInfo(const juce::String& trackUuid,
                      const juce::String& pluginInstance,
                      const juce::String& trackName,
                      const juce::String& trackType,
                      int mcuChannel,
                      int groupId = 0);

    /** Set callback for when the middleware sends an updated group list. */
    void setGroupListCallback(GroupListCallback cb) { onGroupList = std::move(cb); }

    bool isConnected() const { return connected.load(); }

    static constexpr int kReconnectMs  = 5000;
    static constexpr int kHeartbeatMs  = 30000;

private:
    void run() override;
    bool tryConnect();
    void sendRegister();
    void sendHeartbeat();
    void sendJson(const juce::String& json);
    void processIncoming(const juce::String& line);

    juce::String host;
    int          port = 9001;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { false };

    std::mutex  metaMutex;
    juce::String trackUuid;
    juce::String pluginInstance;
    juce::String trackName;
    juce::String trackType;
    int          mcuChannel = -1;
    int          groupId    = 0;

    std::atomic<bool> needsSend { false };
    double lastHeartbeatTime = 0.0;

    GroupListCallback onGroupList;
};
