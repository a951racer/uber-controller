// ChannelReporter.h
// Manages a TCP connection to the middleware.
// Sends full channel mixer state (volume, pan, mute, solo, name, type, index).
// Receives group list and mixer commands from the middleware.
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

// Commands from middleware to plugin
struct MixerCommand
{
    enum Type { SetVolume, SetPan, SetMute, SetSolo, SetSelect };
    Type   type;
    double value = 0.0;
    bool   flag  = false;
};

class ChannelReporter : private juce::Thread
{
public:
    using GroupListCallback  = std::function<void(const std::vector<GroupInfo>&)>;
    using MixerCmdCallback   = std::function<void(const MixerCommand&)>;

    ChannelReporter();
    ~ChannelReporter() override;

    void start(const juce::String& host, int port);
    void stop();

    /** Send full channel state. Only sends if something changed. */
    void setChannelState(const juce::String& trackUuid,
                         const juce::String& pluginInstance,
                         const juce::String& name,
                         const juce::String& type,
                         int channelIndex,
                         int groupId,
                         double volume,
                         double maxVolume,
                         double pan,
                         bool mute,
                         bool solo,
                         bool selected);

    void setGroupListCallback(GroupListCallback cb) { onGroupList = std::move(cb); }
    void setMixerCmdCallback(MixerCmdCallback cb) { onMixerCmd = std::move(cb); }

    bool isConnected() const { return connected.load(); }

    static constexpr int kReconnectMs = 5000;
    static constexpr int kHeartbeatMs = 30000;

private:
    void run() override;
    bool tryConnect();
    void sendState();
    void sendHeartbeat();
    void sendJson(const juce::String& json);
    void processIncoming(const juce::String& line);

    juce::String host;
    int          port = 9001;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { false };

    // Current state (protected by mutex)
    std::mutex   stateMutex;
    juce::String trackUuid;
    juce::String pluginInstance;
    juce::String channelName;
    juce::String channelType;
    int          channelIndex = -1;
    int          groupId      = 0;
    double       volume       = 1.0;
    double       maxVolume    = 1.0;
    double       pan          = 0.5;
    bool         mute         = false;
    bool         solo         = false;
    bool         selected     = false;

    std::atomic<bool> needsSend { false };
    double lastHeartbeatTime = 0.0;

    GroupListCallback onGroupList;
    MixerCmdCallback  onMixerCmd;
};
