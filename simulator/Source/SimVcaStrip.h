// SimVcaStrip.h
// A VCA/Group fader strip. Shows group name + fader.
// Sends VcaFaderMove messages when the fader is moved.
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"

class SimVcaStrip : public juce::Component
{
public:
    SimVcaStrip(int groupId, const juce::String& name, Sim::MessageBus& bus);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    void setGroupName(const juce::String& name);
    void setFaderValue(float v);
    int  getGroupId() const { return groupId; }

private:
    int              groupId;
    juce::String     groupName;
    float            value = 0.5f;  // 0.5 = unity (center)
    Sim::MessageBus& bus;

    float yToValue(int y);
};
