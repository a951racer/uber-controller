// McuProtocol.h  (middleware)
// Stateless helpers for encoding/decoding the Mackie Control Universal protocol.
#pragma once
#include <JuceHeader.h>
#include "../../Shared/SharedMessages.h"

struct McuProtocol
{
    // -----------------------------------------------------------------------
    // Fader  (controller -> DAW)
    // -----------------------------------------------------------------------

    static juce::MidiMessage encodeFaderMove(int faderId, float normValue)
    {
        int raw = juce::roundToInt(normValue * 16383.0f);
        raw = juce::jlimit(0, 16383, raw);
        return juce::MidiMessage(0xE0 | (faderId & 0x07), raw & 0x7F, (raw >> 7) & 0x7F);
    }

    static juce::MidiMessage encodeFaderTouch(int faderId, bool touched)
    {
        int note = 0x68 + (faderId & 0x07);
        if (touched)
            return juce::MidiMessage::noteOn(1, note, (juce::uint8)127);
        else
            return juce::MidiMessage::noteOff(1, note, (juce::uint8)0);
    }

    // -----------------------------------------------------------------------
    // Buttons  (controller -> DAW)
    // All MCU buttons are Note On/Off on channel 1.
    // -----------------------------------------------------------------------

    static juce::MidiMessage encodeButtonPress(int noteNumber, bool pressed)
    {
        if (pressed)
            return juce::MidiMessage::noteOn(1, noteNumber, (juce::uint8)127);
        else
            return juce::MidiMessage::noteOff(1, noteNumber, (juce::uint8)0);
    }

    // -----------------------------------------------------------------------
    // VPot rotation  (controller -> DAW)
    // CC 0x10-0x17, value: 0x01-0x0F = CW ticks, 0x41-0x4F = CCW ticks
    // -----------------------------------------------------------------------

    static juce::MidiMessage encodeVPotTurn(int vpotId, int delta)
    {
        int cc = 0x10 + (vpotId & 0x07);
        int value;
        if (delta > 0)
            value = juce::jlimit(1, 15, delta);         // 0x01-0x0F
        else
            value = 0x40 + juce::jlimit(1, 15, -delta); // 0x41-0x4F
        return juce::MidiMessage::controllerEvent(1, cc, value);
    }

    // -----------------------------------------------------------------------
    // SysEx handshake
    // -----------------------------------------------------------------------

    static bool isDeviceInquiry(const juce::MidiMessage& m, juce::uint8& deviceId)
    {
        if (!m.isSysEx()) return false;
        const juce::uint8* d = m.getSysExData();
        int sz = m.getSysExDataSize();
        if (sz < 4)       return false;
        if (d[0] != 0x7E) return false;
        if (d[2] != 0x06) return false;
        if (d[3] != 0x01) return false;
        deviceId = d[1];
        return true;
    }

    static juce::MidiMessage buildIdentityReply(juce::uint8 deviceId, juce::uint8 mcuDeviceId = 0x14)
    {
        const juce::uint8 reply[] = {
            0xF0,
            0x7E, deviceId, 0x06, 0x02,
            0x00, 0x00, 0x66,
            mcuDeviceId, 0x00,
            0x00, 0x01, 0x00, 0x00,
            0xF7
        };
        return juce::MidiMessage(reply, (int)sizeof(reply));
    }

    // -----------------------------------------------------------------------
    // Fader decoding  (DAW -> controller)
    // -----------------------------------------------------------------------

    static bool decodeFaderMove(const juce::MidiMessage& m, int& faderId, float& normValue)
    {
        if (!m.isPitchWheel()) return false;
        int channel = m.getChannel() - 1;
        if (channel < 0 || channel > 7) return false;

        const juce::uint8* data = m.getRawData();
        int lsb = data[1] & 0x7F;
        int msb = data[2] & 0x7F;
        int raw = (msb << 7) | lsb;

        faderId   = channel;
        normValue = static_cast<float>(raw) / 16383.0f;
        return true;
    }

    // -----------------------------------------------------------------------
    // Button LED decoding  (DAW -> controller)
    // DAW sends Note On vel=127 (LED on) or vel=0 / Note Off (LED off)
    // on channel 1 for any MCU button note.
    // -----------------------------------------------------------------------

    static bool decodeButtonLed(const juce::MidiMessage& m, int& noteNumber, bool& ledOn)
    {
        if (m.getChannel() != 1) return false;

        if (m.isNoteOn())
        {
            noteNumber = m.getNoteNumber();
            ledOn      = (m.getVelocity() > 0);
            return true;
        }
        if (m.isNoteOff())
        {
            noteNumber = m.getNoteNumber();
            ledOn      = false;
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // VPot ring decoding  (DAW -> controller)
    // CC 0x30-0x37 on channel 1
    // Value: bits 5-4 = mode, bits 3-0 = position (0-11)
    // -----------------------------------------------------------------------

    static bool decodeVPotRing(const juce::MidiMessage& m, int& vpotId,
                               Sim::VPotMode& mode, int& position)
    {
        if (!m.isController()) return false;
        if (m.getChannel() != 1) return false;

        int cc = m.getControllerNumber();
        if (cc < 0x30 || cc > 0x37) return false;

        vpotId   = cc - 0x30;
        int val  = m.getControllerValue();
        mode     = static_cast<Sim::VPotMode>((val >> 4) & 0x03);
        position = val & 0x0F;
        return true;
    }

    // -----------------------------------------------------------------------
    // LCD SysEx decoding  (DAW -> controller)
    // F0 00 00 66 14 12 [offset] [chars...] F7
    // -----------------------------------------------------------------------

    static bool decodeLcd(const juce::MidiMessage& m, int& offset,
                          char* text, int& length)
    {
        if (!m.isSysEx()) return false;

        const juce::uint8* d = m.getSysExData();
        int sz = m.getSysExDataSize();

        // Minimum: 00 00 66 [14|15] 12 offset + at least 1 char = 7 bytes
        if (sz < 7) return false;
        if (d[0] != 0x00 || d[1] != 0x00 || d[2] != 0x66) return false;
        if (d[3] != 0x14 && d[3] != 0x15) return false;  // MCU main or extender
        if (d[4] != 0x12) return false;  // LCD write command

        offset = d[5];
        length = sz - 6;
        if (length > 112) length = 112;

        for (int i = 0; i < length; ++i)
            text[i] = static_cast<char>(d[6 + i]);

        return true;
    }
};
