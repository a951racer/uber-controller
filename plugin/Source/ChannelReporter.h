// ChannelReporter.h
// Manages a TCP connection to the middleware and sends channel metadata.
// Runs a background thread for non-blocking I/O.
#pragma once
#include <JuceHeader.h>
#include <functional>
#include <mutex>
#include <atomic>

class ChannelReporter : private juce::Thread
{
public:
    ChannelReporter();
    ~ChannelReporter() override;

    /** Start the reporter. Connects to middleware at host:port. */
    void start(const juce::String& host, int port);
    void stop();

    /** Update the metadata to report. Thread-safe.
        Only triggers a send if values actually changed. */
    void setTrackInfo(const juce::String& trackUuid,
                      const juce::String& pluginInstance,
                      const juce::String& trackName,
                      const juce::String& trackType,
                      int mcuChannel);

    bool isConnected() const { return connected.load(); }

    // Connection settings
    static constexpr int kReconnectMs  = 5000;
    static constexpr int kHeartbeatMs  = 30000;

private:
    void run() override;
    bool tryConnect();
    void sendRegister();
    void sendHeartbeat();
    void sendJson(const juce::String& json);

    juce::String host;
    int          port = 9001;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { false };

    // Track metadata (protected by mutex)
    std::mutex  metaMutex;
    juce::String trackUuid;
    juce::String pluginInstance;
    juce::String trackName;
    juce::String trackType;
    int          mcuChannel = -1;

    std::atomic<bool> needsSend { false };
    double lastHeartbeatTime = 0.0;
};
