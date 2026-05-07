// ControllerClient.cpp
#include "ControllerClient.h"

ControllerClient::ControllerClient() : juce::Thread("ControllerClient") {}

ControllerClient::~ControllerClient()
{
    stop();
}

void ControllerClient::start(const juce::String& h, int p, MessageCallback cb)
{
    host      = h;
    port      = p;
    onMessage = std::move(cb);
    startThread();
}

void ControllerClient::stop()
{
    alive.reset();
    connected = false;
    if (socket) socket->close();
    stopThread(4000);
}

void ControllerClient::send(const Sim::Message& msg)
{
    auto frame = Protocol::encode(msg);
    if (frame.empty()) return;

    std::lock_guard<std::mutex> lock(sendMutex);
    sendQueue.push_back(std::move(frame));
}

void ControllerClient::run()
{
    while (!threadShouldExit())
    {
        if (!connected)
        {
            if (!tryConnect())
            {
                juce::Thread::sleep(kReconnectMs);
                continue;
            }
        }

        handleConnected();
    }
}

bool ControllerClient::tryConnect()
{
    socket = std::make_unique<juce::StreamingSocket>();

    if (!socket->connect(host, port, 2000))
    {
        socket.reset();
        return false;
    }

    connected = true;
    DBG("ControllerClient: connected to " << host << ":" << port);
    return true;
}

void ControllerClient::handleConnected()
{
    Protocol::FrameParser parser([this](const Sim::Message& msg)
    {
        std::weak_ptr<bool> weakAlive = alive;
        Sim::Message copy = msg;
        juce::MessageManager::callAsync([this, copy, weakAlive]()
        {
            if (weakAlive.lock() && onMessage)
                onMessage(copy);
        });
    });

    uint8_t buf[256];

    while (!threadShouldExit() && socket && socket->isConnected())
    {
        // --- Send queued outbound frames ---
        {
            std::lock_guard<std::mutex> lock(sendMutex);
            while (!sendQueue.empty())
            {
                auto& frame = sendQueue.front();
                int written = socket->write(frame.data(), (int)frame.size());
                if (written < 0) { connected = false; return; }
                sendQueue.pop_front();
            }
        }

        // --- Read inbound data (5ms timeout) ---
        if (socket->waitUntilReady(true, 5) > 0)
        {
            int n = socket->read(buf, sizeof(buf), false);
            if (n <= 0)
            {
                connected = false;
                DBG("ControllerClient: disconnected");
                return;
            }
            parser.feed(buf, (size_t)n);
        }
    }

    connected = false;
}
