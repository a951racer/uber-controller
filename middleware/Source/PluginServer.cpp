// PluginServer.cpp
#include "PluginServer.h"
#include <iostream>
#include <algorithm>

#if JUCE_WINDOWS
  #pragma comment(lib, "ws2_32.lib")
#endif

PluginServer::PluginServer(TrackRegistry& reg) : registry(reg) {}
PluginServer::~PluginServer() { stop(); }

bool PluginServer::start(int port)
{
    listenPort = port;

#if JUCE_WINDOWS
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCK) return false;

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(listenSock, 8) != 0)
    {
#if JUCE_WINDOWS
        closesocket(listenSock);
#else
        close(listenSock);
#endif
        listenSock = INVALID_SOCK;
        return false;
    }

    running = true;
    acceptThread = std::thread([this]() { acceptLoop(); });
    std::cout << "[Plugin] Listening on port " << listenPort << std::endl;
    return true;
}

void PluginServer::stop()
{
    running = false;
    if (listenSock != INVALID_SOCK)
    {
#if JUCE_WINDOWS
        closesocket(listenSock);
#else
        close(listenSock);
#endif
        listenSock = INVALID_SOCK;
    }

    if (acceptThread.joinable()) acceptThread.join();

    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        for (auto sock : allClients)
        {
#if JUCE_WINDOWS
            closesocket(sock);
#else
            close(sock);
#endif
        }
        allClients.clear();
        channelToSocket.clear();
    }

    for (auto& t : clientThreads)
        if (t.joinable()) t.join();
    clientThreads.clear();
}

bool PluginServer::sendToChannel(int channelIndex, const std::string& json)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    auto it = channelToSocket.find(channelIndex);
    if (it == channelToSocket.end()) return false;

    send(it->second, json.c_str(), (int)json.size(), 0);
    return true;
}

void PluginServer::broadcastJson(const std::string& json)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto sock : allClients)
        send(sock, json.c_str(), (int)json.size(), 0);
}

void PluginServer::acceptLoop()
{
    while (running)
    {
        sockaddr_in clientAddr = {};
#if JUCE_WINDOWS
        int addrLen = sizeof(clientAddr);
#else
        socklen_t addrLen = sizeof(clientAddr);
#endif
        SocketHandle clientSock = accept(listenSock, (sockaddr*)&clientAddr, &addrLen);
        if (clientSock == INVALID_SOCK)
        {
            if (!running) break;
            continue;
        }

        std::cout << "[Plugin] Client connected" << std::endl;

        {
            std::lock_guard<std::mutex> lock(clientMutex);
            allClients.push_back(clientSock);
        }

        clientThreads.emplace_back([this, clientSock]() { clientLoop(clientSock); });
    }
}

void PluginServer::clientLoop(SocketHandle clientSock)
{
#if JUCE_WINDOWS
    DWORD timeout = 1000;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = { 1, 0 };
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::string lineBuffer;
    std::string lastTrackUuid;
    int clientChannelIndex = -1;
    char buf[2048];

    while (running)
    {
        int n = recv(clientSock, buf, sizeof(buf) - 1, 0);

        if (n > 0)
        {
            buf[n] = '\0';
            lineBuffer += buf;

            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos)
            {
                std::string line = lineBuffer.substr(0, pos);
                lineBuffer = lineBuffer.substr(pos + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                    line.pop_back();
                if (!line.empty())
                    processLine(line, lastTrackUuid, clientSock, clientChannelIndex);
            }
        }
        else if (n == 0)
        {
            std::cout << "[Plugin] Client disconnected (ch " << clientChannelIndex << ")" << std::endl;
            break;
        }
        else
        {
#if JUCE_WINDOWS
            if (WSAGetLastError() == WSAETIMEDOUT) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            break;
        }
    }

    // Cleanup
    if (!lastTrackUuid.empty())
        registry.removeTrack(juce::String(lastTrackUuid));

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        allClients.erase(std::remove(allClients.begin(), allClients.end(), clientSock), allClients.end());
        if (clientChannelIndex >= 0)
            channelToSocket.erase(clientChannelIndex);
    }

#if JUCE_WINDOWS
    closesocket(clientSock);
#else
    close(clientSock);
#endif
}

void PluginServer::processLine(const std::string& line, std::string& lastTrackUuid,
                               SocketHandle clientSock, int& clientChannelIndex)
{
    auto parsed = juce::JSON::parse(juce::String(line));
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    juce::String cmd = obj->getProperty("cmd").toString();

    if (cmd == "channelState" || cmd == "register" || cmd == "update")
    {
        TrackInfo info;
        info.trackUuid      = obj->getProperty("track_uuid").toString();
        info.pluginInstance  = obj->getProperty("plugin_instance").toString();
        info.name           = obj->getProperty("name").toString();
        info.type           = obj->getProperty("type").toString();
        info.groupId        = static_cast<int>(obj->getProperty("group_id"));

        if (obj->hasProperty("channel_index"))
            info.mcuChannel = static_cast<int>(obj->getProperty("channel_index"));
        else if (obj->hasProperty("mcu_channel"))
            info.mcuChannel = static_cast<int>(obj->getProperty("mcu_channel"));

        if (obj->hasProperty("volume"))
            info.volume = static_cast<double>(obj->getProperty("volume"));
        if (obj->hasProperty("max_volume"))
            info.maxVolume = static_cast<double>(obj->getProperty("max_volume"));
        if (obj->hasProperty("pan"))
            info.pan = static_cast<double>(obj->getProperty("pan"));
        if (obj->hasProperty("mute"))
            info.mute = static_cast<int>(obj->getProperty("mute")) != 0;
        if (obj->hasProperty("solo"))
            info.solo = static_cast<int>(obj->getProperty("solo")) != 0;
        if (obj->hasProperty("selected"))
            info.selected = static_cast<int>(obj->getProperty("selected")) != 0;

        lastTrackUuid = info.trackUuid.toStdString();

        // Register this socket for the channel index
        if (info.mcuChannel >= 0)
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            channelToSocket[info.mcuChannel] = clientSock;
            clientChannelIndex = info.mcuChannel;
        }

        // Notify middleware (it will register in the track registry and forward to simulator if changed)
        if (onChannelState)
            onChannelState(info);

        std::cout << "[Plugin] " << cmd.toStdString()
                  << " ch=" << info.mcuChannel
                  << " \"" << info.name.toStdString() << "\""
                  << " vol=" << info.volume
                  << " mute=" << info.mute
                  << " solo=" << info.solo << std::endl;

        // Send current group list to this client
        auto groups = registry.getGroups();
        if (!groups.empty())
        {
            auto* gObj = new juce::DynamicObject();
            gObj->setProperty("cmd", "groupList");
            juce::Array<juce::var> arr;
            for (auto& g : groups)
            {
                auto* item = new juce::DynamicObject();
                item->setProperty("id", g.id);
                item->setProperty("name", g.name);
                arr.add(juce::var(item));
            }
            gObj->setProperty("groups", arr);
            juce::String json = juce::JSON::toString(juce::var(gObj), true).removeCharacters("\r\n") + "\n";
            auto utf8 = json.toUTF8();
            ::send(clientSock, utf8.getAddress(), (int)utf8.sizeInBytes() - 1, 0);
        }
    }
    else if (cmd == "heartbeat")
    {
        juce::String uuid = obj->getProperty("track_uuid").toString();
        juce::String instance = obj->getProperty("plugin_instance").toString();
        int chIdx = -1;
        if (obj->hasProperty("channel_index"))
            chIdx = static_cast<int>(obj->getProperty("channel_index"));
        lastTrackUuid = uuid.toStdString();
        registry.heartbeat(uuid, instance, chIdx);
    }
    else if (cmd == "defineGroups")
    {
        auto* arr = obj->getProperty("groups").getArray();
        if (!arr) return;

        std::vector<TrackRegistry::GroupDef> groups;
        for (auto& item : *arr)
        {
            auto* gObj = item.getDynamicObject();
            if (!gObj) continue;
            TrackRegistry::GroupDef g;
            g.id   = static_cast<int>(gObj->getProperty("id"));
            g.name = gObj->getProperty("name").toString();
            groups.push_back(g);
        }

        registry.setGroups(groups);
        std::cout << "[Plugin] Groups defined: " << groups.size() << std::endl;

        // Broadcast group list to all clients
        auto* gObj = new juce::DynamicObject();
        gObj->setProperty("cmd", "groupList");
        juce::Array<juce::var> groupArr;
        for (auto& g : groups)
        {
            auto* item = new juce::DynamicObject();
            item->setProperty("id", g.id);
            item->setProperty("name", g.name);
            groupArr.add(juce::var(item));
        }
        gObj->setProperty("groups", groupArr);
        std::string jsonStr = juce::JSON::toString(juce::var(gObj), true)
                                  .removeCharacters("\r\n").toStdString() + "\n";
        broadcastJson(jsonStr);

        // Send track assignments to group manager
        auto tracks = registry.getAllTracks();
        auto* tObj = new juce::DynamicObject();
        tObj->setProperty("cmd", "trackAssignments");
        juce::Array<juce::var> trackArr;
        for (auto& t : tracks)
        {
            auto* ti = new juce::DynamicObject();
            ti->setProperty("track_uuid", t.trackUuid);
            ti->setProperty("name", t.name);
            ti->setProperty("mcu_channel", t.mcuChannel);
            ti->setProperty("group_id", t.groupId);
            trackArr.add(juce::var(ti));
        }
        tObj->setProperty("tracks", trackArr);
        juce::String trackJson = juce::JSON::toString(juce::var(tObj), true)
                                     .removeCharacters("\r\n") + "\n";
        auto utf8 = trackJson.toUTF8();
        ::send(clientSock, utf8.getAddress(), (int)utf8.sizeInBytes() - 1, 0);
    }
    else if (cmd == "transportInfo")
    {
        Sim::Message msg;
        msg.type               = Sim::MsgType::TransportInfo;
        msg.transportBpm       = static_cast<double>(obj->getProperty("bpm"));
        msg.transportTimeSigN  = static_cast<int>(obj->getProperty("timeSigNum"));
        msg.transportTimeSigD  = static_cast<int>(obj->getProperty("timeSigDen"));
        msg.transportPpq       = static_cast<double>(obj->getProperty("ppq"));
        msg.transportSamples   = static_cast<int64_t>(static_cast<double>(obj->getProperty("samples")));
        msg.transportSampleRate = static_cast<double>(obj->getProperty("sampleRate"));
        msg.transportPlaying   = static_cast<int>(obj->getProperty("playing")) != 0;
        msg.transportLooping   = static_cast<int>(obj->getProperty("looping")) != 0;
        msg.transportLoopStart = static_cast<double>(obj->getProperty("loopStart"));
        msg.transportLoopEnd   = static_cast<double>(obj->getProperty("loopEnd"));
        msg.transportBar       = static_cast<int>(obj->getProperty("bar"));
        msg.transportBeat      = static_cast<int>(obj->getProperty("beat"));

        if (onTransportInfo)
            onTransportInfo(msg);
    }
}
