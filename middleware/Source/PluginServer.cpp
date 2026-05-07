// PluginServer.cpp
#include "PluginServer.h"
#include <iostream>
#include <sstream>
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
    if (listenSock == INVALID_SOCK)
    {
        std::cout << "[Plugin] Failed to create socket" << std::endl;
        return false;
    }

    // Allow port reuse
    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) != 0)
    {
        std::cout << "[Plugin] Failed to bind on port " << port << std::endl;
#if JUCE_WINDOWS
        closesocket(listenSock);
#else
        close(listenSock);
#endif
        listenSock = INVALID_SOCK;
        return false;
    }

    if (listen(listenSock, 4) != 0)
    {
        std::cout << "[Plugin] Failed to listen on port " << port << std::endl;
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

    if (acceptThread.joinable())
        acceptThread.join();

    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto& t : clientThreads)
        if (t.joinable()) t.join();
    clientThreads.clear();
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
            connectedClients.push_back(clientSock);
        }

        clientThreads.emplace_back([this, clientSock]() { clientLoop(clientSock); });
    }
}

void PluginServer::clientLoop(SocketHandle clientSock)
{
    // Set receive timeout
#if JUCE_WINDOWS
    DWORD timeout = 1000;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::string lineBuffer;
    std::string lastTrackUuid;  // track which UUID this client registered
    char buf[1024];

    while (running)
    {
        int n = recv(clientSock, buf, sizeof(buf) - 1, 0);

        if (n > 0)
        {
            buf[n] = '\0';
            lineBuffer += buf;

            // Process complete lines
            size_t pos;
            while ((pos = lineBuffer.find('\n')) != std::string::npos)
            {
                std::string line = lineBuffer.substr(0, pos);
                lineBuffer = lineBuffer.substr(pos + 1);

                // Trim
                while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                    line.pop_back();

                if (!line.empty())
                {
                    std::cout << "[Plugin] Received: " << line << std::endl;
                    processLine(line, lastTrackUuid, clientSock);
                }
            }
        }
        else if (n == 0)
        {
            std::cout << "[Plugin] Client disconnected (graceful)" << std::endl;
            break;
        }
        else
        {
            // Check for timeout vs real error
#if JUCE_WINDOWS
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT)
                continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
#endif
            std::cout << "[Plugin] Client disconnected (error)" << std::endl;
            break;
        }
    }

    // Plugin removed — deregister from the track registry
    if (!lastTrackUuid.empty())
    {
        std::cout << "[Plugin] Deregistering track uuid=" << lastTrackUuid << std::endl;
        registry.removeTrack(juce::String(lastTrackUuid));
    }

    // Remove from connected clients list
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        connectedClients.erase(
            std::remove(connectedClients.begin(), connectedClients.end(), clientSock),
            connectedClients.end());
    }

#if JUCE_WINDOWS
    closesocket(clientSock);
#else
    close(clientSock);
#endif
}

void PluginServer::processLine(const std::string& line, std::string& lastTrackUuid, SocketHandle clientSock)
{
    auto parsed = juce::JSON::parse(juce::String(line));
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    juce::String cmd = obj->getProperty("cmd").toString();

    if (cmd == "register" || cmd == "update")
    {
        TrackInfo info;
        info.trackUuid      = obj->getProperty("track_uuid").toString();
        info.pluginInstance  = obj->getProperty("plugin_instance").toString();
        info.name           = obj->getProperty("name").toString();
        info.type           = obj->getProperty("type").toString();
        info.mcuChannel     = static_cast<int>(obj->getProperty("mcu_channel"));
        info.groupId        = static_cast<int>(obj->getProperty("group_id"));

        lastTrackUuid = info.trackUuid.toStdString();

        registry.registerTrack(info);

        std::cout << "[Plugin] " << cmd.toStdString() << " track=\"" << info.name.toStdString()
                  << "\" uuid=" << info.trackUuid.toStdString()
                  << " mcu_ch=" << info.mcuChannel
                  << " group=" << info.groupId << std::endl;

        // Send current group list to this client on first register
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
            send(clientSock, utf8.getAddress(), (int)utf8.sizeInBytes() - 1, 0);
        }
    }
    else if (cmd == "heartbeat")
    {
        juce::String uuid     = obj->getProperty("track_uuid").toString();
        juce::String instance = obj->getProperty("plugin_instance").toString();
        int mcuCh             = static_cast<int>(obj->getProperty("mcu_channel"));

        lastTrackUuid = uuid.toStdString();
        registry.heartbeat(uuid, instance, mcuCh);
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

        std::cout << "[Plugin] Groups defined: " << groups.size() << " groups" << std::endl;

        // Broadcast group list to all connected clients
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

        // Also send track assignments to the group manager
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
        send(clientSock, utf8.getAddress(), (int)utf8.sizeInBytes() - 1, 0);
    }
}

void PluginServer::broadcastJson(const std::string& json)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    for (auto sock : connectedClients)
    {
        send(sock, json.c_str(), (int)json.size(), 0);
    }
}
