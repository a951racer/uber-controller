// DebugPanel.cpp
#include "DebugPanel.h"

DebugPanel::DebugPanel()
{
}

void DebugPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    auto bounds = getLocalBounds().reduced(4);

    g.setColour(juce::Colour(0xff00cc44));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));

    // Header
    g.drawText("=== Channel State ===", bounds.removeFromTop(14),
               juce::Justification::centredLeft);

    // Column headers
    g.setColour(juce::Colour(0xff888888));
    g.drawText("Ch  LCD1     LCD2     Fader  VPot    R S M Sel  Plugin Name      Type     MCU",
               bounds.removeFromTop(12), juce::Justification::centredLeft);

    // Per-channel rows
    g.setColour(juce::Colour(0xffcccccc));
    for (int i = 0; i < 24; ++i)
    {
        auto& ch = channels[i];
        juce::String line;
        line << juce::String(i + 1).paddedLeft(' ', 2) << "  ";
        line << ch.lcdRow1.paddedRight(' ', 7).substring(0, 7) << "  ";
        line << ch.lcdRow2.paddedRight(' ', 7).substring(0, 7) << "  ";
        line << juce::String(ch.faderPos, 3).paddedLeft(' ', 5) << "  ";
        line << juce::String(ch.vpotMode) << ":" << juce::String(ch.vpotPos).paddedLeft(' ', 2) << "   ";
        line << (ch.recLed    ? "X" : ".") << " ";
        line << (ch.soloLed   ? "X" : ".") << " ";
        line << (ch.muteLed   ? "X" : ".") << " ";
        line << (ch.selectLed ? "X" : ".") << "  ";
        line << ch.pluginTrackName.paddedRight(' ', 16).substring(0, 16) << " ";
        line << ch.pluginTrackType.paddedRight(' ', 8).substring(0, 8) << " ";
        line << (ch.pluginMcuChannel >= 0 ? juce::String(ch.pluginMcuChannel + 1) : "-");

        g.drawText(line, bounds.removeFromTop(13), juce::Justification::centredLeft);
    }

    // Transport
    bounds.removeFromTop(4);
    g.setColour(juce::Colour(0xff888888));
    juce::String transport = "Transport: ";
    const char* names[] = { "Rew", "FF", "Stop", "Play", "Rec" };
    for (int i = 0; i < 5; ++i)
    {
        transport << names[i] << "=" << (transportLeds[i] ? "ON" : "off") << "  ";
    }
    g.drawText(transport, bounds.removeFromTop(13), juce::Justification::centredLeft);
}

void DebugPanel::resized()
{
    // No child components to layout — everything is painted
}

void DebugPanel::logMessage(const Sim::Message& msg)
{
    switch (msg.type)
    {
        case Sim::MsgType::FaderUpdate:
            if (msg.faderId >= 0 && msg.faderId < 24)
                channels[msg.faderId].faderPos = msg.value;
            break;

        case Sim::MsgType::VPotRingUpdate:
            if (msg.vpotId >= 0 && msg.vpotId < 24)
            {
                channels[msg.vpotId].vpotMode = static_cast<int>(msg.vpotMode);
                channels[msg.vpotId].vpotPos  = msg.vpotPosition;
            }
            break;

        case Sim::MsgType::ButtonLedUpdate:
        {
            int note = msg.buttonNote;

            // Channel strip buttons: Rec 0x00+ch, Solo 0x20+ch, Mute 0x40+ch, Select 0x60+ch
            if (note >= 0x60 && note < 0x60 + 24)
                { int ch = note - 0x60; channels[ch].selectLed = msg.pressed; }
            else if (note >= 0x40 && note < 0x40 + 24)
                { int ch = note - 0x40; channels[ch].muteLed = msg.pressed; }
            else if (note >= 0x20 && note < 0x20 + 24)
                { int ch = note - 0x20; channels[ch].soloLed = msg.pressed; }
            else if (note >= 0x00 && note < 24)
                { int ch = note; channels[ch].recLed = msg.pressed; }

            if (note >= 0x5B && note <= 0x5F)
            {
                int idx = note - 0x5B;
                transportLeds[idx] = msg.pressed;
            }
            break;
        }

        case Sim::MsgType::LcdUpdate:
        {
            int offset = msg.lcdOffset;
            juce::String text(msg.lcdText, msg.lcdLength);

            // Map global LCD offset to channel
            // Each unit: 112 chars (row1: 0-55, row2: 56-111)
            // Global: unit0 at 0-111, unit1 at 112-223, unit2 at 224-335
            for (int ch = 0; ch < 24; ++ch)
            {
                int unit    = ch / 8;
                int localCh = ch % 8;
                int topStart = unit * 112 + localCh * 7;
                int botStart = unit * 112 + 56 + localCh * 7;

                if (offset >= topStart && offset < topStart + 7)
                    channels[ch].lcdRow1 = text;
                else if (offset >= botStart && offset < botStart + 7)
                    channels[ch].lcdRow2 = text;
            }
            break;
        }

        case Sim::MsgType::TrackMeta:
        {
            int ch = msg.trackMcuChannel;
            if (ch >= 0 && ch < 24)
            {
                channels[ch].pluginTrackName  = juce::String(msg.trackName);
                channels[ch].pluginTrackType  = juce::String(msg.trackType);
                channels[ch].pluginMcuChannel = ch;
            }
            break;
        }

        default:
            break;
    }

    repaint();
}
