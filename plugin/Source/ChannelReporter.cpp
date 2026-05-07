// ChannelReporter.cpp
#include "ChannelReporter.h"

#if JUCE_WINDOWS
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <netinet/tcp.h>
  #include <sys/socket.h>
#endif

ChannelReporter::ChannelReporter() : juce::Thread("ChannelReporter") {}

ChannelReporter::~ChannelReporter() { stop(); }

void ChannelReporter::start(const juce::String& h, int p)
{
    host = h;
    port = p;
    startThread();
}

void ChannelReporter::stop()
{
    signalThreadShouldExit();
    connected = false;
    if (socket) socket->close();
    stopThread(5000);
    socket.reset();
}

void ChannelReporter::setTrackInfo(const juce::String& uuid,
                                   const juce::String& instance,
                                   const juce::String& name,
                                   const juce::String& type,
                                   int channel)
{
    std::lock_guard<std::mutex> lock(metaMutex);

    if (trackUuid != uuid || pluginInstance != instance ||
        trackName != name || trackType != type || mcuChannel != channel)
    {
        trackUuid      = uuid;
        pluginInstance = instance;
        trackName      = name;
        trackType      = type;
        mcuChannel     = channel;
        needsSend      = true;
    }
}

void ChannelReporter::run()
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
            sendRegister();
            lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes();
        }

        if (threadShouldExit()) return;

        if (needsSend.exchange(false))
            sendRegister();

        double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastHeartbeatTime >= kHeartbeatMs)
        {
            sendHeartbeat();
            lastHeartbeatTime = now;
        }

        if (threadShouldExit()) return;

        juce::Thread::sleep(100);
    }
}

bool ChannelReporter::tryConnect()
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

void ChannelReporter::sendRegister()
{
    std::lock_guard<std::mutex> lock(metaMutex);

    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", "register");
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstance);
    obj->setProperty("name", trackName);
    obj->setProperty("type", trackType);
    obj->setProperty("mcu_channel", mcuChannel);

    sendJson(juce::JSON::toString(juce::var(obj), true));
}

void ChannelReporter::sendHeartbeat()
{
    std::lock_guard<std::mutex> lock(metaMutex);

    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", "heartbeat");
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstance);
    obj->setProperty("mcu_channel", mcuChannel);

    sendJson(juce::JSON::toString(juce::var(obj), true));
}

void ChannelReporter::sendJson(const juce::String& json)
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
