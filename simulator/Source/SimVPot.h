// SimVPot.h
// A rotary encoder simulation with LED ring display.
// Tracks absolute position (0.0-1.0). Sends absolute value on change.
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
    void mouseUp(const juce::MouseEvent&) override;

    void setRing(Sim::VPotMode mode, int position);

    /** Set the absolute position (0.0-1.0) from external source (DAW feedback). */
    void setPosition(float pos);

private:
    int              vpotId;
    Sim::MessageBus& bus;

    float         position     = 0.5f;  // absolute position 0.0-1.0
    Sim::VPotMode ringMode     = Sim::VPotMode::Single;
    int           ringPosition = 0;

    int lastDragY = 0;
    bool dragging = false;
};
