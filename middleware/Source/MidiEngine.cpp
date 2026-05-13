// MidiEngine.cpp
#include "MidiEngine.h"
#include "McuProtocol.h"
#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// McuUnit
// ---------------------------------------------------------------------------

McuUnit::McuUnit(int idx, DawCallback cb)
    : unitIndex(idx), channelOffset(idx * 8), onMessageFromDaw(std::move(cb))
{
}

McuUnit::~McuUnit()
{
    if (midiIn) midiIn->stop();
}

void McuUnit::openInput(const juce::String& deviceName)
{
    if (midiIn) { midiIn->stop(); midiIn.reset(); }
    if (deviceName.isEmpty()) return;

    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (d.name == deviceName)
        {
            midiIn = juce::MidiInput::openDevice(d.identifier, this);
            if (midiIn) midiIn->start();
            break;
        }
    }
}

void McuUnit::openOutput(const juce::String& deviceName)
{
    midiOut.reset();
    if (deviceName.isEmpty()) return;

    for (auto& d : juce::MidiOutput::getAvailableDevices())
    {
        if (d.name == deviceName)
        {
            midiOut = juce::MidiOutput::openDevice(d.identifier);
            break;
        }
    }
}

void McuUnit::sendToDaw(const Sim::Message& msg, int localFaderId)
{
    if (!midiOut) return;

    switch (msg.type)
    {
        case Sim::MsgType::FaderMove:
            faderValues[localFaderId] = msg.value;
            midiOut->sendMessageNow(McuProtocol::encodeFaderMove(localFaderId, msg.value));
            break;

        case Sim::MsgType::FaderTouch:
            midiOut->sendMessageNow(McuProtocol::encodeFaderTouch(localFaderId, msg.touched));
            break;

        case Sim::MsgType::ButtonPress:
        {
            int note = msg.buttonNote;

            // Transport buttons (0x56-0x5F): send as-is
            if (note >= 0x56 && note <= 0x5F)
            {
                midiOut->sendMessageNow(McuProtocol::encodeButtonPress(note, msg.pressed));
                break;
            }

            // Channel strip buttons: convert global encoding back to local MCU note
            int localNote;
            if (note >= 0x60)       localNote = 0x18 + localFaderId;  // Select
            else if (note >= 0x40)  localNote = 0x10 + localFaderId;  // Mute
            else if (note >= 0x20)  localNote = 0x08 + localFaderId;  // Solo
            else                    localNote = 0x00 + localFaderId;  // Rec

            midiOut->sendMessageNow(McuProtocol::encodeButtonPress(localNote, msg.pressed));
            break;
        }

        case Sim::MsgType::VPotTurn:
            midiOut->sendMessageNow(McuProtocol::encodeVPotTurn(localFaderId, msg.vpotDelta));
            break;

        default:
            break;
    }
}

void McuUnit::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    // SysEx handshake
    if (m.isSysEx())
    {
        juce::uint8 deviceId = 0;
        if (McuProtocol::isDeviceInquiry(m, deviceId))
        {
            if (midiOut)
            {
                // Unit 0 = MCU main (0x14), units 1+ = MCU extender (0x15)
                juce::uint8 mcuDeviceId = (unitIndex == 0) ? 0x14 : 0x15;
                midiOut->sendMessageNow(McuProtocol::buildIdentityReply(deviceId, mcuDeviceId));
            }
        }

        // LCD update — offset by unit
        int  offset = 0;
        char text[128] = {};
        int  length = 0;
        if (McuProtocol::decodeLcd(m, offset, text, length))
        {
            std::cout << "[MCU " << unitIndex << "] LCD offset=" << offset
                      << " len=" << length << " global=" << (offset + unitIndex * 112) << std::endl;
            for (int i = 0; i < length; i += 7)
            {
                Sim::Message out;
                out.type      = Sim::MsgType::LcdUpdate;
                out.lcdOffset = offset + i + (unitIndex * 112);  // global offset
                out.lcdLength = std::min(7, length - i);
                for (int j = 0; j < out.lcdLength; ++j)
                    out.lcdText[j] = text[i + j];
                out.lcdText[out.lcdLength] = '\0';

                if (onMessageFromDaw)
                    onMessageFromDaw(out);
            }
        }

        // Forward raw SysEx for debug
        Sim::Message raw;
        raw.type       = Sim::MsgType::RawSysEx;
        raw.rawDataLen = std::min(m.getRawDataSize(), 64);
        std::memcpy(raw.rawData, m.getRawData(), raw.rawDataLen);
        if (onMessageFromDaw)
            onMessageFromDaw(raw);

        return;
    }

    // Fader position from DAW
    int   faderId   = 0;
    float normValue = 0.0f;
    if (McuProtocol::decodeFaderMove(m, faderId, normValue))
    {
        if (std::abs(normValue - faderValues[faderId]) < 1.0f / 16383.0f)
            return;
        faderValues[faderId] = normValue;

        Sim::Message out;
        out.type    = Sim::MsgType::FaderUpdate;
        out.faderId = faderId + channelOffset;  // global channel ID
        out.value   = normValue;

        if (onMessageFromDaw)
            onMessageFromDaw(out);
        return;
    }

    // VPot ring from DAW
    int vpotId = 0;
    Sim::VPotMode vpotMode;
    int vpotPos = 0;
    if (McuProtocol::decodeVPotRing(m, vpotId, vpotMode, vpotPos))
    {
        Sim::Message out;
        out.type         = Sim::MsgType::VPotRingUpdate;
        out.vpotId       = vpotId + channelOffset;
        out.vpotMode     = vpotMode;
        out.vpotPosition = vpotPos;

        if (onMessageFromDaw)
            onMessageFromDaw(out);
        return;
    }

    // Button LED from DAW
    int  noteNumber = 0;
    bool ledOn      = false;
    if (McuProtocol::decodeButtonLed(m, noteNumber, ledOn))
    {
        Sim::Message out;
        out.type       = Sim::MsgType::ButtonLedUpdate;
        out.pressed    = ledOn;

        // Channel strip buttons: remap local note to global
        if (noteNumber >= 0x00 && noteNumber <= 0x1F)
        {
            int localCh = noteNumber & 0x07;
            int btnBase = noteNumber & 0x18;

            // Remap to non-overlapping global ranges:
            // MCU Rec (0x00-0x07)    -> global 0x00 + globalCh
            // MCU Solo (0x08-0x0F)   -> global 0x20 + globalCh
            // MCU Mute (0x10-0x17)   -> global 0x40 + globalCh
            // MCU Select (0x18-0x1F) -> global 0x60 + globalCh
            int globalCh = localCh + channelOffset;
            switch (btnBase)
            {
                case 0x00: out.buttonNote = 0x00 + globalCh; break;  // Rec
                case 0x08: out.buttonNote = 0x20 + globalCh; break;  // Solo
                case 0x10: out.buttonNote = 0x40 + globalCh; break;  // Mute
                case 0x18: out.buttonNote = 0x60 + globalCh; break;  // Select
                default:   out.buttonNote = noteNumber; break;
            }
        }
        else
        {
            out.buttonNote = noteNumber;
        }

        if (onMessageFromDaw)
            onMessageFromDaw(out);
    }
}

// ---------------------------------------------------------------------------
// MidiEngine
// ---------------------------------------------------------------------------

MidiEngine::MidiEngine() {}

void MidiEngine::init(const std::vector<McuUnitConfig>& units)
{
    for (int i = 0; i < (int)units.size(); ++i)
    {
        auto unit = std::make_unique<McuUnit>(i, [this](const Sim::Message& msg)
        {
            if (onMessageFromDaw)
                onMessageFromDaw(msg);
        });

        unit->openInput(units[i].midiIn);
        unit->openOutput(units[i].midiOut);

        std::cout << "[MCU " << i << "] In: " << units[i].midiIn
                  << "  Out: " << units[i].midiOut << std::endl;

        mcuUnits.push_back(std::move(unit));
    }
}

void MidiEngine::sendToDaw(const Sim::Message& msg)
{
    int globalCh = -1;

    switch (msg.type)
    {
        case Sim::MsgType::FaderMove:
        case Sim::MsgType::FaderTouch:
            globalCh = msg.faderId;
            break;

        case Sim::MsgType::VPotTurn:
            globalCh = msg.vpotId;
            break;

        case Sim::MsgType::ButtonPress:
        {
            int note = msg.buttonNote;

            // Transport buttons first (0x56-0x5F) — send directly on unit 0
            if (note >= 0x56 && note <= 0x5F)
            {
                if (!mcuUnits.empty())
                    mcuUnits[0]->sendToDaw(msg, 0);
                return;
            }

            // Decode global button note to find globalCh
            // Rec: 0x00+ch, Solo: 0x20+ch, Mute: 0x40+ch, Select: 0x60+ch
            if (note >= 0x60 && note < 0x60 + 24)
                globalCh = note - 0x60;
            else if (note >= 0x40 && note < 0x40 + 24)
                globalCh = note - 0x40;
            else if (note >= 0x20 && note < 0x20 + 24)
                globalCh = note - 0x20;
            else if (note >= 0x00 && note < 0x00 + 24)
                globalCh = note - 0x00;
            else
            {
                // Other global buttons — send on unit 0
                if (!mcuUnits.empty())
                    mcuUnits[0]->sendToDaw(msg, 0);
                return;
            }
            break;
        }

        default:
            return;
    }

    if (globalCh < 0) return;

    int unitIdx   = globalCh / 8;
    int localFader = globalCh % 8;

    if (unitIdx >= 0 && unitIdx < (int)mcuUnits.size())
        mcuUnits[unitIdx]->sendToDaw(msg, localFader);
}
