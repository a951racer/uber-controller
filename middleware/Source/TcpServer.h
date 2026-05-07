// TcpServer.h
// Listens on a TCP port. Accepts multiple clients (simulator + any debug tools).
// Each connected client gets a TcpClientConnection.
#pragma once
#include <JuceHeader.h>
#include "ClientConnection.h"
#include <vector>
#include <mutex>
#include <functional>

class TcpClientConnection : public ClientConnection,
                            private juce::Thread
{
public:
    TcpClientConnection(std::unique_ptr<juce::StreamingSocket> sock,
                        MessageCallback cb);
    ~TcpClientConnection() override;

    void send(const Sim::Message& msg) override;
    bool isConnected() const override { return connected.load(); }

private:
    void run() override;

    std::unique_ptr<juce::StreamingSocket> socket;
    std::atomic<bool> connected { true };

    std::mutex              sendMutex;
    std::deque<std::vector<uint8_t>> sendQueue;
};

// ---------------------------------------------------------------------------

class TcpServer : private juce::Thread
{
public:
    using MessageCallback = std::function<void(const Sim::Message&)>;

    TcpServer();
    ~TcpServer() override;

    bool start(int port, MessageCallback cb);
    void stop();

    /** Broadcast a message to all connected TCP clients. */
    void broadcast(const Sim::Message& msg);

private:
    void run() override;
    void pruneDisconnected();

    int           listenPort = 8888;
    MessageCallback onMessage;

    std::unique_ptr<juce::StreamingSocket> listenSocket;

    std::mutex mutex;
    std::vector<std::unique_ptr<TcpClientConnection>> clients;
};
