// PluginServer.h
// TCP server for DAW channel agent plugins.
// Tracks which client socket corresponds to which channel index.
// Routes commands from the middleware to specific plugins by channel.
#pragma once
#include <JuceHeader.h>
#include "TrackRegistry.h"
#include <vector>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <map>
#include <algorithm>

#if JUCE_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  using SocketHandle = int;
  #define INVALID_SOCK (-1)
#endif

class PluginServer
{
public:
    using ChannelStateCallback = std::function<void(const TrackInfo&)>;

    PluginServer(TrackRegistry& registry);
    ~PluginServer();

    bool start(int port);
    void stop();

    bool sendToChannel(int channelIndex, const std::string& json);
    void broadcastJson(const std::string& json);

    /** Called when a plugin reports channel state — middleware uses this to update the simulator. */
    void setChannelStateCallback(ChannelStateCallback cb) { onChannelState = std::move(cb); }

private:
    void acceptLoop();
    void clientLoop(SocketHandle clientSock);
    void processLine(const std::string& line, std::string& lastTrackUuid,
                     SocketHandle clientSock, int& clientChannelIndex);

    TrackRegistry& registry;
    int listenPort = 9001;

    SocketHandle listenSock = INVALID_SOCK;
    std::atomic<bool> running { false };

    std::thread acceptThread;
    std::vector<std::thread> clientThreads;

    // Map channel index → socket for targeted sends
    std::mutex clientMutex;
    std::map<int, SocketHandle> channelToSocket;
    std::vector<SocketHandle> allClients;

    ChannelStateCallback onChannelState;
};
