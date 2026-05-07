// PluginServer.h
// TCP server that accepts connections from DAW channel agent plugins.
// Parses newline-delimited JSON and updates the TrackRegistry.
// Uses raw Winsock/POSIX sockets to avoid JUCE socket quirks.
#pragma once
#include <JuceHeader.h>
#include "TrackRegistry.h"
#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <string>

#if JUCE_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <fcntl.h>
  using SocketHandle = int;
  #define INVALID_SOCK (-1)
#endif

class PluginServer
{
public:
    PluginServer(TrackRegistry& registry);
    ~PluginServer();

    bool start(int port);
    void stop();

private:
    void acceptLoop();
    void clientLoop(SocketHandle clientSock);
    void processLine(const std::string& line, std::string& lastTrackUuid);

    TrackRegistry& registry;
    int listenPort = 9001;

    SocketHandle listenSock = INVALID_SOCK;
    std::atomic<bool> running { false };

    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    std::mutex clientMutex;
};
