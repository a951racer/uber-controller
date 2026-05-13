// TransportInfoPanel.h
// Displays session/transport: time (HH:MM:SS.mmm), bars (BBB.BB.TT), BPM, loop.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/SharedMessages.h"
#include <cmath>

class TransportInfoPanel : public juce::Component
{
public:
    TransportInfoPanel();

    void paint(juce::Graphics&) override;
    void update(const Sim::Message& msg);

private:
    double  bpm        = 120.0;
    int     timeSigN   = 4;
    int     timeSigD   = 4;
    int     bar        = 1;
    int     beat       = 1;
    double  ppq        = 0.0;
    int64_t samples    = 0;
    double  sampleRate = 44100.0;
    bool    playing    = false;
    bool    looping    = false;
    double  loopStart  = 0.0;
    double  loopEnd    = 0.0;
};
