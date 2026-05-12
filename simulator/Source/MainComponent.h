// MainComponent.h
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"
#include "ControllerClient.h"
#include "SimChannelStrip.h"
#include "SimTransport.h"
#include "SimVcaStrip.h"
#include "DebugPanel.h"

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;

private:
    void timerCallback() override;

    static constexpr int kTotalChannels = 24;

    Sim::MessageBus uiBus;

    ControllerClient client;
    SimTransport     transport;

    std::vector<std::unique_ptr<SimChannelStrip>> strips;

    // Scrollable viewport for channel strips
    juce::Viewport   stripViewport;
    juce::Component  stripContainer;

    juce::Label      statusLabel { {}, "Disconnected" };
    juce::TextButton debugToggle { "Debug" };
    DebugPanel       debugPanel;
    bool             debugVisible = false;

    // Channel type filter buttons
    std::vector<std::unique_ptr<juce::TextButton>> filterButtons;
    juce::String activeFilter;

    // Per-channel type tracking (from plugin metadata)
    juce::String channelTypes[24];

    // VCA fader strips
    std::vector<std::unique_ptr<SimVcaStrip>> vcaStrips;
    juce::Component vcaContainer;
    void updateVcaStrips();

    // LCD buffer: 2 rows × 56 chars per MCU unit × 3 units
    char lcdBuffer[336] = {};  // 3 * 112

    void handleUiMessage(const Sim::Message& msg);
    void handleServerMessage(const Sim::Message& msg);
    void updateLcdFromBuffer(int offset, int length);
    void applyFilter();
    void setActiveFilter(const juce::String& filter);
    void layoutStrips();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
