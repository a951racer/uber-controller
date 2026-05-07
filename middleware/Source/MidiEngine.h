// MidiEngine.h
// Manages multiple MCU units (up to 3: main + 2 extenders).
// Each unit handles 8 channels. Channel IDs in messages are global (0-23).
// The engine maps global channel IDs to the correct unit + local fader (0-7).
#pragma once
#include <JuceHeader.h>
#include "../../Shared/SharedMessages.h"
#include "Config.h"
#include <functional>
#include <vector>
#include <memory>

class McuUnit : public juce::MidiInputCallback
{
public:
    using DawCallback = std::function<void(const Sim::Message&)>;

    McuUnit(int unitIndex, DawCallback cb);
    ~McuUnit() override;

    void openInput(const juce::String& deviceName);
    void openOutput(const juce::String& deviceName);

    void sendToDaw(const Sim::Message& msg, int localFaderId);

    int getUnitIndex() const { return unitIndex; }

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    int unitIndex;  // 0, 1, or 2
    int channelOffset;  // unitIndex * 8

    DawCallback onMessageFromDaw;

    std::unique_ptr<juce::MidiInput>  midiIn;
    std::unique_ptr<juce::MidiOutput> midiOut;

    float faderValues[8] = {};
};

// ---------------------------------------------------------------------------

class MidiEngine
{
public:
    using DawCallback = std::function<void(const Sim::Message&)>;

    MidiEngine();

    void setDawCallback(DawCallback cb) { onMessageFromDaw = std::move(cb); }

    /** Initialize with config. Creates 1-3 MCU units. */
    void init(const std::vector<McuUnitConfig>& units);

    /** Send a message to the DAW. Global channel ID (0-23) is mapped to the correct unit. */
    void sendToDaw(const Sim::Message& msg);

    int getNumUnits() const { return (int)mcuUnits.size(); }
    int getTotalChannels() const { return getNumUnits() * 8; }

private:
    DawCallback onMessageFromDaw;
    std::vector<std::unique_ptr<McuUnit>> mcuUnits;
};
