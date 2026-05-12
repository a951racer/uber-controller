// MiddlewareApp.h
// Routes data between simulator/hardware, DAW plugins, and MCU (transport only).
#pragma once
#include <JuceHeader.h>
#include "MidiEngine.h"
#include "TcpServer.h"
#include "SerialServer.h"
#include "PluginServer.h"
#include "MeterReceiver.h"
#include "TrackRegistry.h"
#include "Config.h"

class MiddlewareApp : private juce::Timer
{
public:
    MiddlewareApp();
    ~MiddlewareApp();

    void start(const Config& cfg);
    void stop();

private:
    void timerCallback() override;

    // Simulator/hardware sends a message (fader move, button press, etc.)
    void onClientMessage(const Sim::Message& msg);

    // MCU sends transport data (play/stop LED feedback)
    void onDawMessage(const Sim::Message& msg);

    // Track registry changed (plugin registered/updated)
    void onTrackRegistryChanged();

    // VCA fader logic
    void handleVcaFaderMove(int groupId, float newValue);
    float vcaPositions[8] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

    // Send current mixer state for all tracks to the simulator
    void broadcastMixerState();

    // Send state for one channel
    void sendChannelState(const TrackInfo& track);

    // Send track metadata to simulator
    void broadcastTrackMeta();

    // Send track assignments to group manager plugin
    void broadcastTrackAssignments();

    // Route a simulator command to the correct plugin
    void routeToPlugin(int channelIndex, const juce::String& cmd, double value);

    MidiEngine    midiEngine;      // MCU — transport only
    TcpServer     tcpServer;       // simulator/hardware (binary protocol)
    SerialServer  serialServer;    // hardware serial (optional)
    PluginServer  pluginServer;    // DAW plugins (JSON protocol)
    MeterReceiver meterReceiver;   // UDP meter receiver
    TrackRegistry trackRegistry;

    int totalChannels = 24;
};
