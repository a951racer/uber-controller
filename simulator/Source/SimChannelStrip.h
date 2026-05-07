// SimChannelStrip.h
// One channel strip: LCD label (2 rows × 7 chars), VPot, Rec/Solo/Mute/Select buttons, Fader.
#pragma once
#include <JuceHeader.h>
#include "MessageBus.h"
#include "SimFader.h"
#include "SimVPot.h"

class SimChannelStrip : public juce::Component
{
public:
    SimChannelStrip(int channelId, Sim::MessageBus& bus);

    void resized() override;
    void paint(juce::Graphics&) override;

    // External updates from middleware
    void setFaderValue(float v);
    void setVPotRing(Sim::VPotMode mode, int position);
    void setButtonLed(int noteNumber, bool on);
    void setLcdText(int row, const juce::String& text);
    void setPluginName(const juce::String& name);

    int getChannelId() const { return channelId; }

private:
    int              channelId;
    Sim::MessageBus& bus;

    // Sub-components
    SimFader fader;
    SimVPot  vpot;

    // Channel buttons
    struct ChButton : public juce::Component
    {
        int              noteNumber;
        juce::String     label;
        bool             ledOn = false;
        bool             held  = false;
        Sim::MessageBus& bus;
        juce::Colour     ledColour;

        ChButton(int note, const juce::String& lbl, juce::Colour col, Sim::MessageBus& b)
            : noteNumber(note), label(lbl), bus(b), ledColour(col) {}

        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(ledOn ? ledColour : juce::Colour(0xff303030));
            g.fillRoundedRectangle(r, 3.0f);
            if (held)
            {
                g.setColour(juce::Colours::white.withAlpha(0.2f));
                g.fillRoundedRectangle(r, 3.0f);
            }
            g.setColour(juce::Colours::white);
            g.setFont(9.0f);
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

    ChButton recBtn;
    ChButton soloBtn;
    ChButton muteBtn;
    ChButton selectBtn;

    // LCD text (2 rows × 7 chars)
    juce::String lcdRow1;
    juce::String lcdRow2;

    // Plugin-provided name (overrides lcdRow1 when set)
    juce::String pluginName;
};
