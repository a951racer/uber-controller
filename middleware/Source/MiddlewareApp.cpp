// MiddlewareApp.cpp
#include "MiddlewareApp.h"
#include "../../Shared/SharedMessages.h"
#include <cstring>
#include <iostream>

MiddlewareApp::MiddlewareApp()
    : pluginServer(trackRegistry)
{
}

MiddlewareApp::~MiddlewareApp() { stop(); }

void MiddlewareApp::start(const Config& cfg)
{
    // --- MIDI (multi-unit) ---
    midiEngine.setDawCallback([this](const Sim::Message& msg) { onDawMessage(msg); });
    midiEngine.init(cfg.mcuUnits);

    // --- TCP server for simulator/hardware (binary protocol) ---
    tcpServer.start(cfg.tcpPort, [this](const Sim::Message& msg) { onClientMessage(msg); });

    // --- Serial server for hardware (optional) ---
    if (!cfg.serialPort.empty())
        serialServer.start(cfg.serialPort, cfg.serialBaud,
                           [this](const Sim::Message& msg) { onClientMessage(msg); });

    // --- Plugin server (JSON protocol) ---
    pluginServer.start(9001);

    // --- Track registry change callback ---
    trackRegistry.setChangeCallback([this]() { onTrackRegistryChanged(); });

    // --- Periodic prune of stale tracks ---
    startTimerHz(1);

    std::cout << "[Middleware] " << cfg.mcuUnits.size() << " MCU unit(s), "
              << cfg.totalChannels() << " channels" << std::endl;
}

void MiddlewareApp::stop()
{
    stopTimer();
    tcpServer.stop();
    serialServer.stop();
    pluginServer.stop();
}

void MiddlewareApp::timerCallback()
{
    trackRegistry.pruneStale(90000.0);
}

void MiddlewareApp::onClientMessage(const Sim::Message& msg)
{
    midiEngine.sendToDaw(msg);
}

void MiddlewareApp::onDawMessage(const Sim::Message& msg)
{
    tcpServer.broadcast(msg);
    if (serialServer.isConnected())
        serialServer.send(msg);
}

void MiddlewareApp::onTrackRegistryChanged()
{
    broadcastTrackMeta();
    broadcastTrackAssignments();
}

void MiddlewareApp::broadcastTrackAssignments()
{
    // Send track assignments to all plugin clients (Group Manager uses this for the matrix)
    auto tracks = trackRegistry.getAllTracks();

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

    std::string json = juce::JSON::toString(juce::var(tObj), true)
                           .removeCharacters("\r\n").toStdString() + "\n";
    pluginServer.broadcastJson(json);
}

void MiddlewareApp::broadcastTrackMeta()
{
    auto tracks = trackRegistry.getAllTracks();
    int totalCh = midiEngine.getTotalChannels();

    bool hasPlugin[24] = {};

    for (auto& track : tracks)
    {
        if (track.mcuChannel >= 0 && track.mcuChannel < totalCh)
            hasPlugin[track.mcuChannel] = true;

        Sim::Message msg;
        msg.type            = Sim::MsgType::TrackMeta;
        msg.trackMcuChannel = track.mcuChannel;

        std::strncpy(msg.trackName, track.name.toRawUTF8(), 31);
        msg.trackName[31] = '\0';

        std::strncpy(msg.trackType, track.type.toRawUTF8(), 15);
        msg.trackType[15] = '\0';

        std::strncpy(msg.trackUuid, track.trackUuid.toRawUTF8(), 39);
        msg.trackUuid[39] = '\0';

        tcpServer.broadcast(msg);
        if (serialServer.isConnected())
            serialServer.send(msg);
    }

    // Send empty TrackMeta for channels without a plugin
    for (int ch = 0; ch < totalCh; ++ch)
    {
        if (!hasPlugin[ch])
        {
            Sim::Message msg;
            msg.type            = Sim::MsgType::TrackMeta;
            msg.trackMcuChannel = ch;
            msg.trackName[0]    = '\0';
            msg.trackType[0]    = '\0';
            msg.trackUuid[0]    = '\0';

            tcpServer.broadcast(msg);
            if (serialServer.isConnected())
                serialServer.send(msg);
        }
    }
}
