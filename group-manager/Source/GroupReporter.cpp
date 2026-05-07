// GroupReporter.cpp
#include "GroupReporter.h"

#if JUCE_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <netinet/tcp.h>
  #include <sys/socket.h>
#endif

GroupReporter::GroupReporter() : juce::Thread("GroupReporter") {}
GroupReporter::~GroupReporter() { stop(); }

void GroupReporter::start(const juce::String& h, int p)
{
    host = h;
    port = p;
    startThread();
}

void GroupReporter::stop()
{
    signalThreadShouldExit();
    connected = false;
    if (socket) socket->close();
    stopThread(5000);
    socket.reset();
}

void GroupReporter::setGroups(const std::vector<GroupDef>& groups)
{
    std::lock_guard<std::mutex> lock(mutex);
    currentGroups = groups;
    needsSend = true;
}

void GroupReporter::run()
{
    while (!threadShouldExit())
    {
        if (!connected)
        {
            if (threadShouldExit()) return;
            if (!tryConnect())
            {
                for (int i = 0; i < kReconnectMs / 100 && !threadShouldExit(); ++i)
                    juce::Thread::sleep(100);
                continue;
            }
            sendGroupDefs();
        }

        if (threadShouldExit()) return;

        if (needsSend.exchange(false))
            sendGroupDefs();

        // Read incoming data from middleware (track assignments for matrix)
        if (socket && connected)
        {
            char buf[2048];
            int n = socket->read(buf, sizeof(buf) - 1, false);
            if (n > 0)
            {
                buf[n] = '\0';
                // Process line by line
                juce::String data(buf, n);
                static juce::String lineBuffer;
                lineBuffer += data;

                int pos;
                while ((pos = lineBuffer.indexOf("\n")) >= 0)
                {
                    juce::String line = lineBuffer.substring(0, pos).trim();
                    lineBuffer = lineBuffer.substring(pos + 1);
                    if (line.isNotEmpty())
                        processIncoming(line);
                }
            }
            else if (n < 0)
            {
                // Check if it's just "no data" or actual disconnect
                // On Windows, non-blocking read returns -1 for "would block"
            }
        }

        if (threadShouldExit()) return;
        juce::Thread::sleep(100);
    }
}

bool GroupReporter::tryConnect()
{
    socket = std::make_unique<juce::StreamingSocket>();
    if (!socket->connect(host, port, 3000))
    {
        socket.reset();
        return false;
    }

    int rawHandle = socket->getRawSocketHandle();
    if (rawHandle != -1)
    {
        int flag = 1;
        setsockopt(rawHandle, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
    }

    connected = true;
    return true;
}

void GroupReporter::sendGroupDefs()
{
    std::lock_guard<std::mutex> lock(mutex);

    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", "defineGroups");
    obj->setProperty("plugin_type", "group_manager");

    juce::Array<juce::var> groupArray;
    for (auto& g : currentGroups)
    {
        auto* gObj = new juce::DynamicObject();
        gObj->setProperty("id", g.id);
        gObj->setProperty("name", g.name);
        groupArray.add(juce::var(gObj));
    }
    obj->setProperty("groups", groupArray);

    sendJson(juce::JSON::toString(juce::var(obj), true));
}

void GroupReporter::sendJson(const juce::String& json)
{
    if (!socket || !connected) return;

    juce::String line = json.removeCharacters("\r\n") + "\n";
    auto utf8 = line.toUTF8();
    int len = (int)utf8.sizeInBytes() - 1;

    int written = socket->write(utf8.getAddress(), len);
    if (written < 0)
    {
        connected = false;
        socket.reset();
    }
}

void GroupReporter::processIncoming(const juce::String& line)
{
    auto parsed = juce::JSON::parse(line);
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    juce::String cmd = obj->getProperty("cmd").toString();

    if (cmd == "trackAssignments")
    {
        auto* arr = obj->getProperty("tracks").getArray();
        if (!arr) return;

        std::vector<TrackAssignment> assignments;
        for (auto& item : *arr)
        {
            auto* tObj = item.getDynamicObject();
            if (!tObj) continue;

            TrackAssignment ta;
            ta.trackUuid  = tObj->getProperty("track_uuid").toString();
            ta.trackName  = tObj->getProperty("name").toString();
            ta.mcuChannel = static_cast<int>(tObj->getProperty("mcu_channel"));
            ta.groupId    = static_cast<int>(tObj->getProperty("group_id"));
            assignments.push_back(ta);
        }

        if (onTrackUpdate)
        {
            juce::MessageManager::callAsync([this, assignments]()
            {
                if (onTrackUpdate)
                    onTrackUpdate(assignments);
            });
        }
    }
}
