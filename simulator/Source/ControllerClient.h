// ControllerClient.h
// TCP client that connects to the middleware.
// Replaces the old in-process Middleware class.
// Auto-reconnects every kReconnectMs if the connection drops.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/Protocol.h"
#include <functional>
#include <atomic>
#include <mutex>
#include <deque>

class ControllerClient : private juce::Thread
{
public:
    using MessageCallback = std::function<void(const Sim::Message&)>;

    static constexpr int kReconnectMs = 3000;

    ControllerClient();
    ~ControllerClient() override;

    /** Start the client thread. cb is called (on the JUCE message thread)
        whenever the middleware sends a message. */
    void start(const juce::String& host, int port, MessageCallback cb);
    void stop();

    /** Send a message to the middleware. Thread-safe. */
    void send(const Sim::Message& msg);

    bool isConnected() const { return connected.load(); }

private:
    void run() override;
    bool tryConnect();
    void handleConnected();

    juce::String    host;
    int             port = 8888;
    MessageCallback onMessage;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { false };

    std::mutex sendMutex;
    std::deque<std::vector<uint8_t>> sendQueue;

    std::shared_ptr<bool> alive { std::make_shared<bool>(true) };
};
