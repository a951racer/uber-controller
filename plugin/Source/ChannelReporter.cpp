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

void ChannelReporter::setChannelState(const juce::String& uuid,
                                      const juce::String& instance,
                                      const juce::String& name,
                                      const juce::String& type,
                                      int index,
                                      int group,
                                      double vol,
                                      double maxVol,
                                      double p,
                                      bool m,
                                      bool s,
                                      bool sel)
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (trackUuid != uuid || pluginInstance != instance ||
        channelName != name || channelType != type ||
        channelIndex != index || groupId != group ||
        std::abs(volume - vol) > 0.001 || std::abs(pan - p) > 0.005 ||
        mute != m || solo != s || selected != sel ||
        std::abs(maxVolume - maxVol) > 0.001)
    {
        trackUuid      = uuid;
        pluginInstance = instance;
        channelName    = name;
        channelType    = type;
        channelIndex   = index;
        groupId        = group;
        volume         = vol;
        maxVolume      = maxVol;
        pan            = p;
        mute           = m;
        solo           = s;
        selected       = sel;
        needsSend      = true;
    }
}

void ChannelReporter::run()
{
    juce::String lineBuffer;

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
            sendState();
            lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes();
        }

        if (threadShouldExit()) return;

        if (needsSend.exchange(false))
            sendState();

        double now = juce::Time::getMillisecondCounterHiRes();
        if (now - lastHeartbeatTime >= kHeartbeatMs)
        {
            sendHeartbeat();
            lastHeartbeatTime = now;
        }

        // Read incoming data from middleware
        if (socket && connected)
        {
            char buf[1024];
            int n = socket->read(buf, sizeof(buf) - 1, false);
            if (n > 0)
            {
                buf[n] = '\0';
                lineBuffer += juce::String::fromUTF8(buf, n);

                int pos;
                while ((pos = lineBuffer.indexOf("\n")) >= 0)
                {
                    juce::String line = lineBuffer.substring(0, pos).trim();
                    lineBuffer = lineBuffer.substring(pos + 1);
                    if (line.isNotEmpty())
                        processIncoming(line);
                }
            }
        }

        if (threadShouldExit()) return;
        juce::Thread::sleep(50);
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

void ChannelReporter::sendState()
{
    std::lock_guard<std::mutex> lock(stateMutex);

    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", "channelState");
    obj->setProperty("plugin_type", "channel_agent");
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstance);
    obj->setProperty("name", channelName);
    obj->setProperty("type", channelType);
    obj->setProperty("channel_index", channelIndex);
    obj->setProperty("group_id", groupId);
    obj->setProperty("volume", volume);
    obj->setProperty("max_volume", maxVolume);
    obj->setProperty("pan", pan);
    obj->setProperty("mute", mute);
    obj->setProperty("solo", solo);
    obj->setProperty("selected", selected);

    sendJson(juce::JSON::toString(juce::var(obj), true));
}

void ChannelReporter::sendHeartbeat()
{
    std::lock_guard<std::mutex> lock(stateMutex);

    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", "heartbeat");
    obj->setProperty("plugin_type", "channel_agent");
    obj->setProperty("track_uuid", trackUuid);
    obj->setProperty("plugin_instance", pluginInstance);
    obj->setProperty("channel_index", channelIndex);

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

void ChannelReporter::processIncoming(const juce::String& line)
{
    auto parsed = juce::JSON::parse(line);
    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    juce::String cmd = obj->getProperty("cmd").toString();

    if (cmd == "groupList")
    {
        auto* arr = obj->getProperty("groups").getArray();
        if (!arr) return;

        std::vector<GroupInfo> groups;
        for (auto& item : *arr)
        {
            auto* gObj = item.getDynamicObject();
            if (!gObj) continue;
            GroupInfo gi;
            gi.id   = static_cast<int>(gObj->getProperty("id"));
            gi.name = gObj->getProperty("name").toString();
            groups.push_back(gi);
        }

        if (onGroupList)
        {
            juce::MessageManager::callAsync([this, groups]()
            {
                if (onGroupList)
                    onGroupList(groups);
            });
        }
    }
    else if (cmd == "setVolume")
    {
        if (onMixerCmd)
        {
            MixerCommand mc;
            mc.type  = MixerCommand::SetVolume;
            mc.value = static_cast<double>(obj->getProperty("value"));
            juce::MessageManager::callAsync([this, mc]() { if (onMixerCmd) onMixerCmd(mc); });
        }
    }
    else if (cmd == "setPan")
    {
        if (onMixerCmd)
        {
            MixerCommand mc;
            mc.type  = MixerCommand::SetPan;
            mc.value = static_cast<double>(obj->getProperty("value"));
            juce::MessageManager::callAsync([this, mc]() { if (onMixerCmd) onMixerCmd(mc); });
        }
    }
    else if (cmd == "setMute")
    {
        if (onMixerCmd)
        {
            MixerCommand mc;
            mc.type = MixerCommand::SetMute;
            mc.flag = static_cast<int>(obj->getProperty("value")) != 0;
            juce::MessageManager::callAsync([this, mc]() { if (onMixerCmd) onMixerCmd(mc); });
        }
    }
    else if (cmd == "setSolo")
    {
        if (onMixerCmd)
        {
            MixerCommand mc;
            mc.type = MixerCommand::SetSolo;
            mc.flag = static_cast<int>(obj->getProperty("value")) != 0;
            juce::MessageManager::callAsync([this, mc]() { if (onMixerCmd) onMixerCmd(mc); });
        }
    }
    else if (cmd == "setSelect")
    {
        if (onMixerCmd)
        {
            MixerCommand mc;
            mc.type = MixerCommand::SetSelect;
            mc.flag = static_cast<int>(obj->getProperty("value")) != 0;
            juce::MessageManager::callAsync([this, mc]() { if (onMixerCmd) onMixerCmd(mc); });
        }
    }
}
