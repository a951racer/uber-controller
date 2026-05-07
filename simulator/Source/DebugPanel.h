// DebugPanel.h
// Shows per-channel state received from the DAW via the middleware.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/SharedMessages.h"
#include <array>

class DebugPanel : public juce::Component
{
public:
    DebugPanel();

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Call this for every message received from the middleware. */
    void logMessage(const Sim::Message& msg);

private:
    struct ChannelState
    {
        juce::String lcdRow1;
        juce::String lcdRow2;
        float        faderPos  = 0.0f;
        int          vpotMode  = 0;
        int          vpotPos   = 0;
        bool         recLed    = false;
        bool         soloLed   = false;
        bool         muteLed   = false;
        bool         selectLed = false;
        juce::String pluginTrackName;
        juce::String pluginTrackType;
        int          pluginMcuChannel = -1;
    };

    std::array<ChannelState, 24> channels;

    bool transportLeds[5] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugPanel)
};
