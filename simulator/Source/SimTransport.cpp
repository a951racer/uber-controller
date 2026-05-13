// SimTransport.cpp
#include "SimTransport.h"

SimTransport::SimTransport(Sim::MessageBus& bus)
{
    using TB = Sim::TransportButton;

    struct Def { int note; const char* label; };
    static const Def defs[] = {
        { static_cast<int>(TB::Rewind),      "Rew"  },
        { static_cast<int>(TB::FastForward), "FF"   },
        { static_cast<int>(TB::Stop),        "Stop" },
        { static_cast<int>(TB::Play),        "Play" },
        { static_cast<int>(TB::Record),      "Rec"  },
        { 0x56,                              "Loop" },
    };

    for (auto& d : defs)
    {
        auto btn = std::make_unique<TransportBtn>(d.note, d.label, bus);
        addAndMakeVisible(*btn);
        buttons.push_back(std::move(btn));
    }
}

void SimTransport::resized()
{
    auto r = getLocalBounds();
    int w  = r.getWidth() / (int)buttons.size();
    for (auto& btn : buttons)
        btn->setBounds(r.removeFromLeft(w).reduced(2));
}

void SimTransport::setLed(Sim::TransportButton button, bool on)
{
    int note = static_cast<int>(button);
    for (auto& btn : buttons)
    {
        if (btn->noteNumber == note)
        {
            btn->ledOn = on;
            btn->repaint();
            return;
        }
    }
}

void SimTransport::setLedByNote(int noteNumber, bool on)
{
    for (auto& btn : buttons)
    {
        if (btn->noteNumber == noteNumber)
        {
            btn->ledOn = on;
            btn->repaint();
            return;
        }
    }
}
