// SharedMessages.h
// Message types shared between the simulator and middleware.
#pragma once
#include <string>
#include <array>
#include <cstdint>

namespace Sim {

enum class MsgType
{
    FaderMove,        // controller -> middleware: user dragged a fader
    FaderTouch,       // controller -> middleware: user touched/released a fader
    FaderUpdate,      // middleware -> controller: DAW sent a new fader position
    ButtonPress,      // controller -> middleware: button pressed/released
    ButtonLedUpdate,  // middleware -> controller: DAW set a button LED state
    VPotTurn,         // controller -> middleware: user turned a VPot
    VPotRingUpdate,   // middleware -> controller: DAW set VPot LED ring
    LcdUpdate,        // middleware -> controller: DAW sent LCD text
    RawSysEx,         // middleware -> controller: raw SysEx for debug display
    TrackMeta,        // middleware -> controller: track metadata from plugin
    MeterUpdate,      // middleware -> controller: channel meter levels
    VcaFaderMove,     // controller -> middleware: VCA fader moved
    VcaFaderUpdate,   // middleware -> controller: VCA fader position (for sync)
    TransportInfo,    // middleware -> controller: session/transport state
    MidiDeviceChange  // (internal, not sent over wire)
};

// ---- Button IDs -----------------------------------------------------------
// Transport buttons
enum class TransportButton
{
    Rewind      = 0x5B,
    FastForward = 0x5C,
    Stop        = 0x5D,
    Play        = 0x5E,
    Record      = 0x5F
};

// Channel strip buttons (per-channel, note = base + globalChannelId)
enum class ChannelButton
{
    Rec    = 0x00,  // 0x00 + ch (0-23)
    Solo   = 0x20,  // 0x20 + ch (0-23)
    Mute   = 0x40,  // 0x40 + ch (0-23)
    Select = 0x60   // 0x60 + ch (0-23)
};

// ---- VPot LED ring modes --------------------------------------------------
// The DAW sends a byte: bits 5-4 = mode, bits 3-0 = position (0-11)
enum class VPotMode : uint8_t
{
    Single = 0,  // single dot
    BoostCut = 1,  // fill from center
    Wrap   = 2,  // fill from left
    Spread = 3   // spread from center
};

// ---- Message struct -------------------------------------------------------
struct Message
{
    MsgType type = MsgType::FaderMove;

    // Fader
    int   faderId  = 0;
    float value    = 0.0f;
    bool  touched  = false;

    // Button (transport or channel strip)
    int  buttonNote = 0;   // raw MCU note number
    bool pressed    = false;

    // Legacy transport button (kept for compatibility)
    TransportButton button = TransportButton::Stop;

    // VPot
    int  vpotId    = 0;    // 0-7
    int  vpotDelta = 0;    // positive = clockwise, negative = counter-clockwise

    // VPot ring (from DAW)
    VPotMode vpotMode     = VPotMode::Single;
    int      vpotPosition = 0;  // 0-11

    // LCD
    int  lcdOffset = 0;    // character offset (0-111)
    char lcdText[8] = {};  // up to 7 chars + null
    int  lcdLength = 0;    // number of chars in this update

    // Raw SysEx (for debug display, max 64 bytes)
    uint8_t rawData[64] = {};
    int     rawDataLen   = 0;

    // Track metadata (from plugin via middleware)
    int  trackMcuChannel = -1;
    char trackName[32]   = {};
    char trackType[16]   = {};
    char trackUuid[40]   = {};

    // Meter levels (0-16383, 14-bit)
    int meterChannel = -1;
    int meterPeakL   = 0;
    int meterPeakR   = 0;

    // VCA fader
    int   vcaGroupId = 0;   // which group this VCA controls
    float vcaValue   = 0.5f; // fader position (0.5 = unity/center)

    // Transport/session info
    double transportBpm       = 120.0;
    int    transportTimeSigN  = 4;
    int    transportTimeSigD  = 4;
    double transportPpq       = 0.0;
    int64_t transportSamples  = 0;
    double  transportSampleRate = 44100.0;
    bool   transportPlaying   = false;
    bool   transportLooping   = false;
    double transportLoopStart = 0.0;
    double transportLoopEnd   = 0.0;
    int    transportBar       = 1;
    int    transportBeat      = 1;

    // Internal
    std::string midiDeviceName;
    bool        isMidiInput = false;
};

} // namespace Sim
