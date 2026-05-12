// PslContextBridge.h
// Bridges the PreSonus PSL Context Info extensions to our plugin.
// Provides read/write access to the DAW's mixer state:
//   volume, pan, mute, solo, name, index, type, color, selected
//
// Implements IContextInfoHandler2 to receive change notifications from the host.
// Queries IContextInfoProvider2 from the host's IComponentHandler.
//
#pragma once
#include <JuceHeader.h>

// Include PSL extension headers
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

// We need to include the PSL header directly
// Copy the interface definitions here to avoid path dependencies

namespace Presonus {

struct IContextInfoProvider : Steinberg::FUnknown
{
    virtual Steinberg::tresult PLUGIN_API getContextInfoValue(Steinberg::int32& value, Steinberg::FIDString id) = 0;
    virtual Steinberg::tresult PLUGIN_API getContextInfoString(Steinberg::Vst::TChar* string, Steinberg::int32 maxCharCount, Steinberg::FIDString id) = 0;
    static const Steinberg::FUID iid;
};

struct IContextInfoProvider2 : IContextInfoProvider
{
    using IContextInfoProvider::getContextInfoValue;
    virtual Steinberg::tresult PLUGIN_API getContextInfoValue(double& value, Steinberg::FIDString id) = 0;
    virtual Steinberg::tresult PLUGIN_API setContextInfoValue(Steinberg::FIDString id, double value) = 0;
    virtual Steinberg::tresult PLUGIN_API setContextInfoValue(Steinberg::FIDString id, Steinberg::int32 value) = 0;
    virtual Steinberg::tresult PLUGIN_API setContextInfoString(Steinberg::FIDString id, Steinberg::Vst::TChar* string) = 0;
    static const Steinberg::FUID iid;
};

struct IContextInfoHandler2 : Steinberg::FUnknown
{
    virtual void PLUGIN_API notifyContextInfoChange(Steinberg::FIDString id) = 0;
    static const Steinberg::FUID iid;
};

// Context info attribute IDs
namespace ContextInfo {
    constexpr auto kID         = "id";
    constexpr auto kName       = "name";
    constexpr auto kType       = "type";
    constexpr auto kIndex      = "index";
    constexpr auto kColor      = "color";
    constexpr auto kSelected   = "selected";
    constexpr auto kVolume     = "volume";
    constexpr auto kMaxVolume  = "maxVolume";
    constexpr auto kPan        = "pan";
    constexpr auto kMute       = "mute";
    constexpr auto kSolo       = "solo";
    constexpr auto kVisibility = "visibility";

    enum ChannelType { kTrack = 0, kBus, kFX, kSynth, kIn, kOut };
}

} // namespace Presonus

// ---------------------------------------------------------------------------
// PslContextBridge
// Wraps the PSL interfaces for easy use from the plugin processor.
// ---------------------------------------------------------------------------

#include <functional>
#include <mutex>

struct ChannelMixerState
{
    int          channelIndex = -1;
    juce::String channelName;
    juce::String channelId;       // unique host-assigned ID
    int          channelType  = 0; // Presonus::ContextInfo::ChannelType
    int          channelColor = 0; // RGBA
    double       volume       = 1.0;
    double       maxVolume    = 1.0;
    double       pan          = 0.5;
    bool         mute         = false;
    bool         solo         = false;
    bool         selected     = false;
};

class PslContextBridge
{
public:
    using StateChangeCallback = std::function<void(const ChannelMixerState&)>;

    PslContextBridge();
    ~PslContextBridge();

    /** Called by VST3ClientExtensions when the host provides IComponentHandler. */
    void setComponentHandler(Steinberg::FUnknown* handler);

    /** Get the current mixer state (thread-safe snapshot). */
    ChannelMixerState getState() const;

    /** Set a callback for when the host notifies us of a change. */
    void setStateChangeCallback(StateChangeCallback cb) { onChange = std::move(cb); }

    // --- Write to DAW ---
    bool setVolume(double value);
    bool setPan(double value);
    bool setMute(bool muted);
    bool setSolo(bool soloed);
    bool setSelected(bool selected);

    /** Called by the VST3 wrapper when the host notifies a context change.
        This is the IContextInfoHandler2::notifyContextInfoChange implementation. */
    void handleContextInfoChange(Steinberg::FIDString id);

    /** Returns the IContextInfoHandler2 interface pointer for the host to use. */
    Steinberg::FUnknown* getHandlerInterface();

    bool isAvailable() const { return provider != nullptr; }

private:
    void refreshAll();
    void refreshAttribute(Steinberg::FIDString id);

    Presonus::IContextInfoProvider2* provider = nullptr;

    mutable std::mutex stateMutex;
    ChannelMixerState  state;

    StateChangeCallback onChange;

    // Implementation of IContextInfoHandler2
    class Handler;
    std::unique_ptr<Handler> handler;
};
