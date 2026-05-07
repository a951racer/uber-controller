// SimVPot.h
// A rotary encoder simulation with LED ring display.
// Click and drag up/down to turn. The ring shows the DAW's feedback.
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"

class SimVPot : public juce::Component
{
public:
    SimVPot(int id, Sim::MessageBus& bus);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    void setRing(Sim::VPotMode mode, int position);

private:
    int              vpotId;
    Sim::MessageBus& bus;

    Sim::VPotMode ringMode     = Sim::VPotMode::Single;
    int           ringPosition = 0;  // 0-11

    int lastDragY = 0;
};
