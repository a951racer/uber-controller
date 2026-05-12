// SimFader.h
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"

class SimFader : public juce::Component
{
public:
    SimFader(int id, Sim::MessageBus& bus);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    void setValue(float v);
    void setMeterLevels(float l, float r);

private:
    int   faderId;
    float value   = 0.0f;
    bool  touched = false;
    float meterL  = 0.0f;
    float meterR  = 0.0f;

    Sim::MessageBus& bus;

    float yToValue(int y);
};
