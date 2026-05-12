// PslContextBridge.cpp
#include "PslContextBridge.h"
#include <cstring>

using namespace Steinberg;

// Helper to convert Vst::TChar (char16_t) to juce::String
static juce::String tcharToString(const Vst::TChar* buf)
{
    return juce::String(juce::CharPointer_UTF16(reinterpret_cast<const juce::CharPointer_UTF16::CharType*>(buf)));
}

// Define the IIDs
const FUID Presonus::IContextInfoProvider::iid(0x483e61ea, 0x17994494, 0x8199a35a, 0xebb35e3c);
const FUID Presonus::IContextInfoProvider2::iid(0x61e45968, 0x3d364f39, 0xb15e1733, 0x4944172b);
const FUID Presonus::IContextInfoHandler2::iid(0x31e29a7a, 0xe55043ad, 0x8b95b9b8, 0xda1fbe1e);

// ---------------------------------------------------------------------------
// Handler — implements IContextInfoHandler2 as a COM object
// ---------------------------------------------------------------------------

class PslContextBridge::Handler : public Presonus::IContextInfoHandler2
{
public:
    Handler(PslContextBridge& bridge) : owner(bridge), refCount(1) {}

    // IContextInfoHandler2
    void PLUGIN_API notifyContextInfoChange(FIDString id) override
    {
        owner.handleContextInfoChange(id);
    }

    // FUnknown
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override
    {
        if (FUnknownPrivate::iidEqual(_iid, Presonus::IContextInfoHandler2::iid))
        {
            *obj = static_cast<Presonus::IContextInfoHandler2*>(this);
            addRef();
            return kResultOk;
        }
        if (FUnknownPrivate::iidEqual(_iid, FUnknown::iid))
        {
            *obj = static_cast<FUnknown*>(this);
            addRef();
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }

    uint32 PLUGIN_API addRef() override { return ++refCount; }
    uint32 PLUGIN_API release() override
    {
        uint32 r = --refCount;
        if (r == 0) delete this;
        return r;
    }

private:
    PslContextBridge& owner;
    std::atomic<uint32> refCount;
};

// ---------------------------------------------------------------------------
// PslContextBridge
// ---------------------------------------------------------------------------

PslContextBridge::PslContextBridge()
{
    handler = std::make_unique<Handler>(*this);
}

PslContextBridge::~PslContextBridge() = default;

void PslContextBridge::setComponentHandler(FUnknown* componentHandler)
{
    provider = nullptr;

    if (!componentHandler) return;

    // Query for IContextInfoProvider2
    void* obj = nullptr;
    if (componentHandler->queryInterface(Presonus::IContextInfoProvider2::iid, &obj) == kResultOk && obj)
    {
        provider = static_cast<Presonus::IContextInfoProvider2*>(obj);
        // Don't addRef again — queryInterface already did
        refreshAll();
    }
}

FUnknown* PslContextBridge::getHandlerInterface()
{
    return static_cast<Presonus::IContextInfoHandler2*>(handler.get());
}

ChannelMixerState PslContextBridge::getState() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return state;
}

void PslContextBridge::handleContextInfoChange(FIDString id)
{
    if (id == nullptr || id[0] == '\0')
    {
        // Initial notification — refresh everything
        refreshAll();
    }
    else
    {
        refreshAttribute(id);
    }

    if (onChange)
    {
        ChannelMixerState snapshot;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            snapshot = state;
        }
        onChange(snapshot);
    }
}

// --- Write to DAW ---

bool PslContextBridge::setVolume(double value)
{
    if (!provider) return false;
    return provider->setContextInfoValue(Presonus::ContextInfo::kVolume, value) == kResultOk;
}

bool PslContextBridge::setPan(double value)
{
    if (!provider) return false;
    return provider->setContextInfoValue(Presonus::ContextInfo::kPan, value) == kResultOk;
}

bool PslContextBridge::setMute(bool muted)
{
    if (!provider) return false;
    return provider->setContextInfoValue(Presonus::ContextInfo::kMute, (int32)(muted ? 1 : 0)) == kResultOk;
}

bool PslContextBridge::setSolo(bool soloed)
{
    if (!provider) return false;
    return provider->setContextInfoValue(Presonus::ContextInfo::kSolo, (int32)(soloed ? 1 : 0)) == kResultOk;
}

bool PslContextBridge::setSelected(bool selected)
{
    if (!provider) return false;
    return provider->setContextInfoValue(Presonus::ContextInfo::kSelected, (int32)(selected ? 1 : 0)) == kResultOk;
}

// --- Read from DAW ---

void PslContextBridge::refreshAll()
{
    if (!provider) return;

    std::lock_guard<std::mutex> lock(stateMutex);

    // Index
    int32 idx = -1;
    if (provider->getContextInfoValue(idx, Presonus::ContextInfo::kIndex) == kResultOk)
        state.channelIndex = idx;

    // Name
    Vst::TChar nameBuf[128] = {};
    if (provider->getContextInfoString(nameBuf, 128, Presonus::ContextInfo::kName) == kResultOk)
        state.channelName = tcharToString(nameBuf);

    // ID
    Vst::TChar idBuf[128] = {};
    if (provider->getContextInfoString(idBuf, 128, Presonus::ContextInfo::kID) == kResultOk)
        state.channelId = tcharToString(idBuf);

    // Type
    int32 type = 0;
    if (provider->getContextInfoValue(type, Presonus::ContextInfo::kType) == kResultOk)
        state.channelType = type;

    // Color
    int32 color = 0;
    if (provider->getContextInfoValue(color, Presonus::ContextInfo::kColor) == kResultOk)
        state.channelColor = color;

    // Volume
    double vol = 1.0;
    if (provider->getContextInfoValue(vol, Presonus::ContextInfo::kVolume) == kResultOk)
        state.volume = vol;

    // Max volume
    double maxVol = 1.0;
    if (provider->getContextInfoValue(maxVol, Presonus::ContextInfo::kMaxVolume) == kResultOk)
        state.maxVolume = maxVol;

    // Pan
    double pan = 0.5;
    if (provider->getContextInfoValue(pan, Presonus::ContextInfo::kPan) == kResultOk)
        state.pan = pan;

    // Mute
    int32 mute = 0;
    if (provider->getContextInfoValue(mute, Presonus::ContextInfo::kMute) == kResultOk)
        state.mute = (mute != 0);

    // Solo
    int32 solo = 0;
    if (provider->getContextInfoValue(solo, Presonus::ContextInfo::kSolo) == kResultOk)
        state.solo = (solo != 0);

    // Selected
    int32 sel = 0;
    if (provider->getContextInfoValue(sel, Presonus::ContextInfo::kSelected) == kResultOk)
        state.selected = (sel != 0);
}

void PslContextBridge::refreshAttribute(FIDString id)
{
    if (!provider || !id) return;

    std::lock_guard<std::mutex> lock(stateMutex);

    if (std::strcmp(id, Presonus::ContextInfo::kVolume) == 0)
    {
        double val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.volume = val;
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kPan) == 0)
    {
        double val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.pan = val;
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kMute) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.mute = (val != 0);
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kSolo) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.solo = (val != 0);
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kName) == 0)
    {
        Vst::TChar buf[128] = {};
        if (provider->getContextInfoString(buf, 128, id) == kResultOk)
            state.channelName = tcharToString(buf);
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kIndex) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.channelIndex = val;
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kSelected) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.selected = (val != 0);
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kColor) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.channelColor = val;
    }
    else if (std::strcmp(id, Presonus::ContextInfo::kType) == 0)
    {
        int32 val = 0;
        if (provider->getContextInfoValue(val, id) == kResultOk)
            state.channelType = val;
    }
}
