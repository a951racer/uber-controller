// MeterReceiver.h
// Listens for UDP meter packets from channel agent plugins.
// Forwards them to the simulator via a callback.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/SharedMessages.h"
#include <thread>
#include <atomic>
#include <functional>

#if JUCE_WINDOWS
  #include <winsock2.h>
  using SocketHandle = SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  using SocketHandle = int;
#endif

class MeterReceiver
{
public:
    using MeterCallback = std::function<void(const Sim::Message&)>;

    MeterReceiver();
    ~MeterReceiver();

    bool start(int port, MeterCallback cb);
    void stop();

private:
    void receiveLoop();

    SocketHandle sock = 0;
    std::atomic<bool> running { false };
    std::thread recvThread;
    MeterCallback onMeter;
};
