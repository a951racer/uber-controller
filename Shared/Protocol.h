// Protocol.h
// Binary framing for the controller <-> middleware byte stream.
// Works identically over TCP or USB serial.
//
// Frame layout:
//   [0xAA] [0x55] [type:u8] [payloadLen:u8] [payload...] [xorChecksum:u8]
//
// Wire message types:
//   0x01  FaderMove      faderId(u8)  value_hi(u8)  value_lo(u8)
//   0x02  FaderTouch     faderId(u8)  touched(u8)
//   0x03  FaderUpdate    faderId(u8)  value_hi(u8)  value_lo(u8)
//   0x04  ButtonPress    note(u8)     pressed(u8)
//   0x05  ButtonLed      note(u8)     ledOn(u8)
//   0x06  VPotTurn       vpotId(u8)   delta(i8, signed)
//   0x07  VPotRing       vpotId(u8)   mode(u8)  position(u8)
//   0x08  LcdUpdate      offset(u8)   length(u8)  chars[length]
//
#pragma once
#include "SharedMessages.h"
#include <vector>
#include <cstdint>
#include <functional>
#include <cstring>

namespace Protocol {

static constexpr uint8_t kSyncA       = 0xAA;
static constexpr uint8_t kSyncB       = 0x55;

static constexpr uint8_t kFaderMove   = 0x01;
static constexpr uint8_t kFaderTouch  = 0x02;
static constexpr uint8_t kFaderUpdate = 0x03;
static constexpr uint8_t kButtonPress = 0x04;
static constexpr uint8_t kButtonLed   = 0x05;
static constexpr uint8_t kVPotTurn    = 0x06;
static constexpr uint8_t kVPotRing    = 0x07;
static constexpr uint8_t kLcdUpdate   = 0x08;
static constexpr uint8_t kRawSysEx    = 0x09;
static constexpr uint8_t kTrackMeta   = 0x0A;

// ---------------------------------------------------------------------------
// Encoding
// ---------------------------------------------------------------------------

inline std::vector<uint8_t> encode(const Sim::Message& msg)
{
    uint8_t type = 0;
    uint8_t payload[72] = {};
    uint8_t payLen = 0;

    auto encodeValue = [](float norm, uint8_t& hi, uint8_t& lo)
    {
        int raw = static_cast<int>(norm * 16383.0f + 0.5f);
        if (raw < 0)     raw = 0;
        if (raw > 16383) raw = 16383;
        hi = static_cast<uint8_t>((raw >> 7) & 0x7F);
        lo = static_cast<uint8_t>( raw       & 0x7F);
    };

    switch (msg.type)
    {
        case Sim::MsgType::FaderMove:
            type = kFaderMove;
            payload[0] = static_cast<uint8_t>(msg.faderId);
            encodeValue(msg.value, payload[1], payload[2]);
            payLen = 3;
            break;

        case Sim::MsgType::FaderTouch:
            type = kFaderTouch;
            payload[0] = static_cast<uint8_t>(msg.faderId);
            payload[1] = msg.touched ? 1 : 0;
            payLen = 2;
            break;

        case Sim::MsgType::FaderUpdate:
            type = kFaderUpdate;
            payload[0] = static_cast<uint8_t>(msg.faderId);
            encodeValue(msg.value, payload[1], payload[2]);
            payLen = 3;
            break;

        case Sim::MsgType::ButtonPress:
            type = kButtonPress;
            payload[0] = static_cast<uint8_t>(msg.buttonNote);
            payload[1] = msg.pressed ? 1 : 0;
            payLen = 2;
            break;

        case Sim::MsgType::ButtonLedUpdate:
            type = kButtonLed;
            payload[0] = static_cast<uint8_t>(msg.buttonNote);
            payload[1] = msg.pressed ? 1 : 0;
            payLen = 2;
            break;

        case Sim::MsgType::VPotTurn:
            type = kVPotTurn;
            payload[0] = static_cast<uint8_t>(msg.vpotId);
            payload[1] = static_cast<uint8_t>(static_cast<int8_t>(msg.vpotDelta));
            payLen = 2;
            break;

        case Sim::MsgType::VPotRingUpdate:
            type = kVPotRing;
            payload[0] = static_cast<uint8_t>(msg.vpotId);
            payload[1] = static_cast<uint8_t>(msg.vpotMode);
            payload[2] = static_cast<uint8_t>(msg.vpotPosition);
            payLen = 3;
            break;

        case Sim::MsgType::LcdUpdate:
            type = kLcdUpdate;
            payload[0] = static_cast<uint8_t>((msg.lcdOffset >> 8) & 0xFF);  // offset high byte
            payload[1] = static_cast<uint8_t>(msg.lcdOffset & 0xFF);          // offset low byte
            payload[2] = static_cast<uint8_t>(msg.lcdLength);
            for (int i = 0; i < msg.lcdLength && i < 7; ++i)
                payload[3 + i] = static_cast<uint8_t>(msg.lcdText[i]);
            payLen = static_cast<uint8_t>(3 + msg.lcdLength);
            break;

        case Sim::MsgType::RawSysEx:
        {
            type = kRawSysEx;
            int len = msg.rawDataLen;
            if (len > 64) len = 64;
            payload[0] = static_cast<uint8_t>(len);
            for (int i = 0; i < len; ++i)
                payload[1 + i] = msg.rawData[i];
            payLen = static_cast<uint8_t>(1 + len);
            break;
        }

        case Sim::MsgType::TrackMeta:
        {
            type = kTrackMeta;
            int idx = 0;
            payload[idx++] = static_cast<uint8_t>(msg.trackMcuChannel & 0xFF);

            int nameLen = static_cast<int>(std::strlen(msg.trackName));
            if (nameLen > 31) nameLen = 31;
            payload[idx++] = static_cast<uint8_t>(nameLen);
            for (int i = 0; i < nameLen; ++i)
                payload[idx++] = static_cast<uint8_t>(msg.trackName[i]);

            int typeLen = static_cast<int>(std::strlen(msg.trackType));
            if (typeLen > 15) typeLen = 15;
            payload[idx++] = static_cast<uint8_t>(typeLen);
            for (int i = 0; i < typeLen; ++i)
                payload[idx++] = static_cast<uint8_t>(msg.trackType[i]);

            payLen = static_cast<uint8_t>(idx);
            break;
        }

        default:
            return {};
    }

    uint8_t cs = type ^ payLen;
    for (int i = 0; i < payLen; ++i) cs ^= payload[i];

    std::vector<uint8_t> frame;
    frame.reserve(5 + payLen);
    frame.push_back(kSyncA);
    frame.push_back(kSyncB);
    frame.push_back(type);
    frame.push_back(payLen);
    for (int i = 0; i < payLen; ++i) frame.push_back(payload[i]);
    frame.push_back(cs);
    return frame;
}

// ---------------------------------------------------------------------------
// Decoding — streaming parser
// ---------------------------------------------------------------------------

class FrameParser
{
public:
    using Callback = std::function<void(const Sim::Message&)>;

    explicit FrameParser(Callback cb) : onMessage(std::move(cb)) {}

    void feed(const uint8_t* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
            processByte(data[i]);
    }

    void reset() { state = State::WaitSyncA; }

private:
    enum class State { WaitSyncA, WaitSyncB, ReadType, ReadLen, ReadPayload, ReadChecksum };

    State   state   = State::WaitSyncA;
    uint8_t msgType = 0;
    uint8_t payLen  = 0;
    uint8_t payIdx  = 0;
    uint8_t payload[72] = {};

    Callback onMessage;

    void processByte(uint8_t b)
    {
        switch (state)
        {
            case State::WaitSyncA:
                if (b == kSyncA) state = State::WaitSyncB;
                break;

            case State::WaitSyncB:
                state = (b == kSyncB) ? State::ReadType : State::WaitSyncA;
                break;

            case State::ReadType:
                msgType = b;
                state   = State::ReadLen;
                break;

            case State::ReadLen:
                payLen = b;
                payIdx = 0;
                state  = (payLen == 0) ? State::ReadChecksum : State::ReadPayload;
                break;

            case State::ReadPayload:
                if (payIdx < sizeof(payload))
                    payload[payIdx] = b;
                ++payIdx;
                if (payIdx >= payLen)
                    state = State::ReadChecksum;
                break;

            case State::ReadChecksum:
            {
                uint8_t cs = msgType ^ payLen;
                for (int i = 0; i < payLen; ++i) cs ^= payload[i];
                if (cs == b)
                    dispatch();
                state = State::WaitSyncA;
                break;
            }
        }
    }

    void dispatch()
    {
        auto decodeValue = [&](int hiIdx, int loIdx) -> float
        {
            int raw = ((payload[hiIdx] & 0x7F) << 7) | (payload[loIdx] & 0x7F);
            return static_cast<float>(raw) / 16383.0f;
        };

        Sim::Message msg;

        switch (msgType)
        {
            case kFaderMove:
                if (payLen < 3) return;
                msg.type    = Sim::MsgType::FaderMove;
                msg.faderId = payload[0];
                msg.value   = decodeValue(1, 2);
                break;

            case kFaderTouch:
                if (payLen < 2) return;
                msg.type    = Sim::MsgType::FaderTouch;
                msg.faderId = payload[0];
                msg.touched = payload[1] != 0;
                break;

            case kFaderUpdate:
                if (payLen < 3) return;
                msg.type    = Sim::MsgType::FaderUpdate;
                msg.faderId = payload[0];
                msg.value   = decodeValue(1, 2);
                break;

            case kButtonPress:
                if (payLen < 2) return;
                msg.type       = Sim::MsgType::ButtonPress;
                msg.buttonNote = payload[0];
                msg.pressed    = payload[1] != 0;
                break;

            case kButtonLed:
                if (payLen < 2) return;
                msg.type       = Sim::MsgType::ButtonLedUpdate;
                msg.buttonNote = payload[0];
                msg.pressed    = payload[1] != 0;
                break;

            case kVPotTurn:
                if (payLen < 2) return;
                msg.type     = Sim::MsgType::VPotTurn;
                msg.vpotId   = payload[0];
                msg.vpotDelta = static_cast<int>(static_cast<int8_t>(payload[1]));
                break;

            case kVPotRing:
                if (payLen < 3) return;
                msg.type         = Sim::MsgType::VPotRingUpdate;
                msg.vpotId       = payload[0];
                msg.vpotMode     = static_cast<Sim::VPotMode>(payload[1]);
                msg.vpotPosition = payload[2];
                break;

            case kLcdUpdate:
                if (payLen < 3) return;
                msg.type      = Sim::MsgType::LcdUpdate;
                msg.lcdOffset = (payload[0] << 8) | payload[1];  // 16-bit offset
                msg.lcdLength = payload[2];
                if (msg.lcdLength > 7) msg.lcdLength = 7;
                for (int i = 0; i < msg.lcdLength && (3 + i) < payLen; ++i)
                    msg.lcdText[i] = static_cast<char>(payload[3 + i]);
                msg.lcdText[msg.lcdLength] = '\0';
                break;

            case kRawSysEx:
                if (payLen < 1) return;
                msg.type       = Sim::MsgType::RawSysEx;
                msg.rawDataLen = payload[0];
                if (msg.rawDataLen > 64) msg.rawDataLen = 64;
                for (int i = 0; i < msg.rawDataLen && (1 + i) < payLen; ++i)
                    msg.rawData[i] = payload[1 + i];
                break;

            case kTrackMeta:
            {
                if (payLen < 3) return;
                msg.type = Sim::MsgType::TrackMeta;
                int idx = 0;
                msg.trackMcuChannel = static_cast<int8_t>(payload[idx++]);

                int nameLen = payload[idx++];
                if (nameLen > 31) nameLen = 31;
                for (int i = 0; i < nameLen && idx < payLen; ++i)
                    msg.trackName[i] = static_cast<char>(payload[idx++]);
                msg.trackName[nameLen] = '\0';

                if (idx < payLen)
                {
                    int typeLen = payload[idx++];
                    if (typeLen > 15) typeLen = 15;
                    for (int i = 0; i < typeLen && idx < payLen; ++i)
                        msg.trackType[i] = static_cast<char>(payload[idx++]);
                    msg.trackType[typeLen] = '\0';
                }
                break;
            }

            default:
                return;
        }

        onMessage(msg);
    }
};

} // namespace Protocol
