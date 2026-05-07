// SimTransport.h
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"

class SimTransport : public juce::Component
{
public:
    SimTransport(Sim::MessageBus& bus);

    void resized() override;
    void setLed(Sim::TransportButton button, bool on);

private:
    struct TransportBtn : public juce::Component
    {
        int              noteNumber;
        juce::String     label;
        bool             ledOn = false;
        bool             held  = false;
        Sim::MessageBus& bus;

        TransportBtn(int note, const juce::String& lbl, Sim::MessageBus& b)
            : noteNumber(note), label(lbl), bus(b) {}

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(ledOn ? juce::Colour(0xff00c040) : juce::Colour(0xff303030));
            g.fillRoundedRectangle(r, 4.0f);
            if (held)
            {
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.fillRoundedRectangle(r, 4.0f);
            }
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(r, 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            g.drawText(label, getLocalBounds(), juce::Justification::centred);
        }

        void mouseDown(const juce::MouseEvent&) override
        {
            held = true; repaint();
            Sim::Message m;
            m.type       = Sim::MsgType::ButtonPress;
            m.buttonNote = noteNumber;
            m.pressed    = true;
            bus.publish(m);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            held = false; repaint();
            Sim::Message m;
            m.type       = Sim::MsgType::ButtonPress;
            m.buttonNote = noteNumber;
            m.pressed    = false;
            bus.publish(m);
        }
    };

    std::vector<std::unique_ptr<TransportBtn>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimTransport)
};
