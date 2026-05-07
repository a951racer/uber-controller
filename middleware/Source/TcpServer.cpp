// TcpServer.cpp
#include "TcpServer.h"
#include <iostream>

// ---------------------------------------------------------------------------
// TcpClientConnection
// ---------------------------------------------------------------------------

TcpClientConnection::TcpClientConnection(std::unique_ptr<juce::StreamingSocket> sock,
                                         MessageCallback cb)
    : ClientConnection(std::move(cb)),
      juce::Thread("TcpClient"),
      socket(std::move(sock))
{
    startThread();
}

TcpClientConnection::~TcpClientConnection()
{
    connected = false;
    if (socket) socket->close();
    stopThread(2000);
}

void TcpClientConnection::send(const Sim::Message& msg)
{
    auto frame = Protocol::encode(msg);
    if (frame.empty()) return;

    std::lock_guard<std::mutex> lock(sendMutex);
    sendQueue.push_back(std::move(frame));
}

void TcpClientConnection::run()
{
    uint8_t buf[256];

    while (!threadShouldExit() && socket && socket->isConnected())
    {
        // --- Send queued outbound frames ---
        {
            std::lock_guard<std::mutex> lock(sendMutex);
            while (!sendQueue.empty())
            {
                auto& frame = sendQueue.front();
                socket->write(frame.data(), (int)frame.size());
                sendQueue.pop_front();
            }
        }

        // --- Read inbound data (non-blocking, 5ms timeout) ---
        if (socket->waitUntilReady(true, 5) > 0)
        {
            int n = socket->read(buf, sizeof(buf), false);
            if (n <= 0)
                break;  // disconnected
            parser.feed(buf, (size_t)n);
        }
    }

    connected = false;
}

// ---------------------------------------------------------------------------
// TcpServer
// ---------------------------------------------------------------------------

TcpServer::TcpServer() : juce::Thread("TcpServer") {}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::start(int port, MessageCallback cb)
{
    listenPort = port;
    onMessage  = std::move(cb);

    listenSocket = std::make_unique<juce::StreamingSocket>();
    if (!listenSocket->createListener(port))
    {
        std::cout << "[TCP] Failed to listen on port " << port << std::endl;
        return false;
    }

    std::cout << "[TCP] Listening on port " << port << std::endl;
    startThread();
    return true;
}

void TcpServer::stop()
{
    if (listenSocket) listenSocket->close();
    stopThread(2000);

    std::lock_guard<std::mutex> lock(mutex);
    clients.clear();
}

void TcpServer::broadcast(const Sim::Message& msg)
{
    std::lock_guard<std::mutex> lock(mutex);
    pruneDisconnected();
    for (auto& c : clients)
        c->send(msg);
}

void TcpServer::run()
{
    while (!threadShouldExit())
    {
        // waitForNextConnection blocks until a client connects or timeout
        auto* raw = listenSocket->waitForNextConnection();
        if (raw == nullptr) continue;

        DBG("TcpServer: client connected");
        std::cout << "[TCP] Simulator/hardware connected" << std::endl;

        auto conn = std::make_unique<TcpClientConnection>(
            std::unique_ptr<juce::StreamingSocket>(raw),
            onMessage);

        std::lock_guard<std::mutex> lock(mutex);
        pruneDisconnected();
        clients.push_back(std::move(conn));
    }
}

void TcpServer::pruneDisconnected()
{
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [](const auto& c) { return !c->isConnected(); }),
        clients.end());
}
