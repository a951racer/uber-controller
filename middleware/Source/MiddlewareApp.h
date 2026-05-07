// MiddlewareApp.h
// Wires MidiEngine, TcpServer, SerialServer, PluginServer, and TrackRegistry together.
#pragma once
#include <JuceHeader.h>
#include "MidiEngine.h"
#include "TcpServer.h"
#include "SerialServer.h"
#include "PluginServer.h"
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

    void onClientMessage(const Sim::Message& msg);
    void onDawMessage(const Sim::Message& msg);
    void onTrackRegistryChanged();
    void broadcastTrackMeta();
    void broadcastTrackAssignments();

    MidiEngine    midiEngine;
    TcpServer     tcpServer;
    SerialServer  serialServer;
    PluginServer  pluginServer;
    TrackRegistry trackRegistry;

    // LCD buffer (captured from DAW, may be useful for future features)
    char lcdBuffer[336] = {};
    int  totalChannels  = 24;
};
