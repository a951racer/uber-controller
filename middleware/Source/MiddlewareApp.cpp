// MiddlewareApp.cpp
#include "MiddlewareApp.h"
#include "../../Shared/SharedMessages.h"
#include <cstring>
#include <cmath>
#include <iostream>

MiddlewareApp::MiddlewareApp()
    : pluginServer(trackRegistry)
{
}

MiddlewareApp::~MiddlewareApp() { stop(); }

void MiddlewareApp::start(const Config& cfg)
{
    totalChannels = cfg.totalChannels();

    // MCU (transport only)
    midiEngine.setDawCallback([this](const Sim::Message& msg) { onDawMessage(msg); });
    if (!cfg.mcuUnits.empty())
    {
        std::vector<McuUnitConfig> transportUnit = { cfg.mcuUnits[0] };
        midiEngine.init(transportUnit);
    }

    // TCP server for simulator/hardware
    tcpServer.start(cfg.tcpPort, [this](const Sim::Message& msg) { onClientMessage(msg); });

    // Serial (optional)
    if (!cfg.serialPort.empty())
        serialServer.start(cfg.serialPort, cfg.serialBaud,
                           [this](const Sim::Message& msg) { onClientMessage(msg); });

    // Plugin server
    pluginServer.start(9001);

    // Meter receiver (UDP)
    meterReceiver.start(9002, [this](const Sim::Message& msg)
    {
        static int meterCount = 0;
        if (++meterCount % 100 == 0)
            std::cout << "[Meters] Received " << meterCount << " packets (ch="
                      << msg.meterChannel << " L=" << msg.meterPeakL << " R=" << msg.meterPeakR << ")" << std::endl;

        tcpServer.broadcast(msg);
        if (serialServer.isConnected()) serialServer.send(msg);
    });

    pluginServer.setChannelStateCallback([this](const TrackInfo& track)
    {
        // Use registerTrack which returns true only if something changed
        // (the registry was pre-updated by simulator commands, so echoes return false)
        bool changed = trackRegistry.registerTrack(track);
        if (changed)
            sendChannelState(track);
    });

    // Track registry change callback
    trackRegistry.setChangeCallback([this]() { onTrackRegistryChanged(); });

    startTimerHz(1);
    std::cout << "[Middleware] Transport MCU + Plugin-based channel control" << std::endl;
}

void MiddlewareApp::stop()
{
    stopTimer();
    meterReceiver.stop();
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
    switch (msg.type)
    {
        case Sim::MsgType::FaderMove:
        {
            // Volume is already normalized 0-1 from the simulator
            auto tracks = trackRegistry.getAllTracks();
            for (auto& t : tracks)
            {
                if (t.mcuChannel == msg.faderId)
                {
                    trackRegistry.updateMixerState(t.trackUuid, msg.value,
                                                   t.pan, t.mute, t.solo, t.selected);
                    break;
                }
            }
            routeToPlugin(msg.faderId, "setVolume", msg.value);
            break;
        }

        case Sim::MsgType::FaderTouch:
            break;

        case Sim::MsgType::ButtonPress:
        {
            int note = msg.buttonNote;

            // Transport → MCU (needs both press and release)
            if (note >= 0x5B && note <= 0x5F)
            {
                midiEngine.sendToDaw(msg);
                break;
            }

            if (!msg.pressed) break;  // channel buttons only act on press

            int ch = -1;
            juce::String cmd;
            double value = 0.0;

            if (note >= 0x60)
            {
                ch = note - 0x60;
                cmd = "setSelect";
                value = 1.0;
            }
            else if (note >= 0x40)
            {
                ch = note - 0x40;
                cmd = "setMute";
                auto tracks = trackRegistry.getAllTracks();
                for (auto& t : tracks)
                    if (t.mcuChannel == ch) { value = t.mute ? 0.0 : 1.0; break; }
            }
            else if (note >= 0x20)
            {
                ch = note - 0x20;
                cmd = "setSolo";
                auto tracks = trackRegistry.getAllTracks();
                for (auto& t : tracks)
                    if (t.mcuChannel == ch) { value = t.solo ? 0.0 : 1.0; break; }
            }
            else
            {
                ch = note;
                cmd = "setRec";
                value = 1.0;
            }

            // Update registry silently so DAW echo is suppressed
            if (ch >= 0 && ch < totalChannels)
            {
                auto tracks = trackRegistry.getAllTracks();
                for (auto& t : tracks)
                {
                    if (t.mcuChannel == ch)
                    {
                        bool newMute = (cmd == "setMute") ? (value > 0.5) : t.mute;
                        bool newSolo = (cmd == "setSolo") ? (value > 0.5) : t.solo;
                        bool newSel  = (cmd == "setSelect") ? true : t.selected;
                        trackRegistry.updateMixerState(t.trackUuid, t.volume,
                                                       t.pan, newMute, newSolo, newSel);

                        // Send LED feedback immediately to the simulator
                        Sim::Message ledMsg = {};
                        ledMsg.type       = Sim::MsgType::ButtonLedUpdate;
                        ledMsg.buttonNote = note;
                        ledMsg.pressed    = (cmd == "setMute") ? newMute :
                                            (cmd == "setSolo") ? newSolo :
                                            (cmd == "setSelect") ? newSel : false;
                        tcpServer.broadcast(ledMsg);

                        break;
                    }
                }
                routeToPlugin(ch, cmd, value);
            }
            break;
        }

        case Sim::MsgType::VPotTurn:
        {
            int ch = msg.vpotId;
            double newPan;

            if (msg.vpotDelta == 0)
            {
                // Absolute mode
                newPan = msg.value;
            }
            else
            {
                // Relative mode
                newPan = 0.5;
                auto tracks = trackRegistry.getAllTracks();
                for (auto& t : tracks)
                {
                    if (t.mcuChannel == ch)
                    {
                        newPan = t.pan + (msg.vpotDelta * 0.05);
                        break;
                    }
                }
                newPan = juce::jlimit(0.0, 1.0, newPan);
            }

            // Update registry silently so DAW echo is suppressed
            auto tracks = trackRegistry.getAllTracks();
            for (auto& t : tracks)
            {
                if (t.mcuChannel == ch)
                {
                    trackRegistry.updateMixerState(t.trackUuid, t.volume,
                                                   newPan, t.mute, t.solo, t.selected);
                    break;
                }
            }

            routeToPlugin(ch, "setPan", newPan);
            break;
        }

        default:
            break;
    }
}

void MiddlewareApp::onDawMessage(const Sim::Message& msg)
{
    // Only forward transport LEDs from MCU
    if (msg.type == Sim::MsgType::ButtonLedUpdate)
    {
        int note = msg.buttonNote;
        if (note >= 0x5B && note <= 0x5F)
        {
            tcpServer.broadcast(msg);
            if (serialServer.isConnected()) serialServer.send(msg);
        }
    }
}

void MiddlewareApp::onTrackRegistryChanged()
{
    // Only broadcast metadata and group assignments here.
    // Mixer state (faders, pan, mute, solo) is sent per-channel
    // directly from the plugin server when a channelState arrives.
    broadcastTrackMeta();
    broadcastTrackAssignments();
}

void MiddlewareApp::broadcastMixerState()
{
    auto tracks = trackRegistry.getAllTracks();
    for (auto& track : tracks)
        sendChannelState(track);
}

void MiddlewareApp::sendChannelState(const TrackInfo& track)
{
    int ch = track.mcuChannel;
    if (ch < 0 || ch >= totalChannels) return;

    // Fader (raw PSL volume — may exceed 1.0 for positive dB)
    Sim::Message faderMsg;
    faderMsg.type    = Sim::MsgType::FaderUpdate;
    faderMsg.faderId = ch;
    faderMsg.value   = static_cast<float>(track.volume);
    if (faderMsg.value > 1.0f) faderMsg.value = 1.0f;  // clamp for wire protocol
    tcpServer.broadcast(faderMsg);

    // dB text from normalized volume (0-1 maps to 0 to maxVolume linear)
    Sim::Message lcdMsg = {};
    lcdMsg.type      = Sim::MsgType::LcdUpdate;
    lcdMsg.lcdOffset = ch;
    if (track.volume <= 0.0001)
    {
        std::strncpy(lcdMsg.lcdText, "-inf", 7);
        lcdMsg.lcdLength = 4;
    }
    else
    {
        double linearVol = track.volume * track.maxVolume;
        double dB = 20.0 * std::log10(linearVol);
        juce::String dbStr = juce::String(dB, 1);
        std::strncpy(lcdMsg.lcdText, dbStr.toRawUTF8(), 7);
        lcdMsg.lcdLength = (std::min)(7, (int)dbStr.length());
    }
    lcdMsg.lcdText[lcdMsg.lcdLength] = '\0';
    tcpServer.broadcast(lcdMsg);

    // Mute
    Sim::Message muteMsg = {};
    muteMsg.type       = Sim::MsgType::ButtonLedUpdate;
    muteMsg.buttonNote = 0x40 + ch;
    muteMsg.pressed    = track.mute;
    tcpServer.broadcast(muteMsg);

    // Solo
    Sim::Message soloMsg = {};
    soloMsg.type       = Sim::MsgType::ButtonLedUpdate;
    soloMsg.buttonNote = 0x20 + ch;
    soloMsg.pressed    = track.solo;
    tcpServer.broadcast(soloMsg);

    // Pan
    Sim::Message panMsg = {};
    panMsg.type         = Sim::MsgType::VPotRingUpdate;
    panMsg.vpotId       = ch;
    panMsg.vpotMode     = Sim::VPotMode::BoostCut;
    panMsg.vpotPosition = static_cast<int>(track.pan * 10.0 + 0.5);
    tcpServer.broadcast(panMsg);

    // Select
    Sim::Message selMsg = {};
    selMsg.type       = Sim::MsgType::ButtonLedUpdate;
    selMsg.buttonNote = 0x60 + ch;
    selMsg.pressed    = track.selected;
    tcpServer.broadcast(selMsg);
}

void MiddlewareApp::broadcastTrackMeta()
{
    auto tracks = trackRegistry.getAllTracks();
    for (auto& track : tracks)
    {
        int ch = track.mcuChannel;
        if (ch < 0 || ch >= totalChannels) continue;

        Sim::Message msg = {};
        msg.type            = Sim::MsgType::TrackMeta;
        msg.trackMcuChannel = ch;
        std::strncpy(msg.trackName, track.name.toRawUTF8(), 31);
        msg.trackName[31] = '\0';
        std::strncpy(msg.trackType, track.type.toRawUTF8(), 15);
        msg.trackType[15] = '\0';
        std::strncpy(msg.trackUuid, track.trackUuid.toRawUTF8(), 39);
        msg.trackUuid[39] = '\0';
        tcpServer.broadcast(msg);
    }
}

void MiddlewareApp::broadcastTrackAssignments()
{
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

void MiddlewareApp::routeToPlugin(int channelIndex, const juce::String& cmd, double value)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("cmd", cmd);
    obj->setProperty("value", value);

    std::string json = juce::JSON::toString(juce::var(obj), true)
                           .removeCharacters("\r\n").toStdString() + "\n";

    pluginServer.sendToChannel(channelIndex, json);
}
