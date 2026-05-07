// SimVPot.cpp
#include "SimVPot.h"

SimVPot::SimVPot(int id, Sim::MessageBus& b)
    : vpotId(id), bus(b)
{
}

void SimVPot::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.45f;

    // Background circle
    g.setColour(juce::Colour(0xff202020));
    g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);

    // LED ring: 11 positions spanning 240 degrees (from -120 to +120)
    float startAngle = -120.0f;
    float totalArc   = 240.0f;

    for (int i = 0; i <= 10; ++i)
    {
        float angle = startAngle + (totalArc * i / 10.0f);
        float rad   = angle * juce::MathConstants<float>::pi / 180.0f;

        float dotR = radius * 0.85f;
        float dx   = cx + dotR * std::sin(rad);
        float dy   = cy - dotR * std::cos(rad);

        bool lit = false;

        switch (ringMode)
        {
            case Sim::VPotMode::Single:
                lit = (i == ringPosition);
                break;
            case Sim::VPotMode::BoostCut:
                // Fill from center (position 5) to ringPosition
                lit = (ringPosition >= 5) ? (i >= 5 && i <= ringPosition)
                                          : (i <= 5 && i >= ringPosition);
                break;
            case Sim::VPotMode::Wrap:
                lit = (i <= ringPosition);
                break;
            case Sim::VPotMode::Spread:
                lit = (i >= 5 - ringPosition && i <= 5 + ringPosition);
                break;
        }

        g.setColour(lit ? juce::Colour(0xff00ff80) : juce::Colour(0xff404040));
        g.fillEllipse(dx - 3, dy - 3, 6, 6);
    }

    // Center knob
    g.setColour(juce::Colour(0xff606060));
    g.fillEllipse(cx - radius * 0.4f, cy - radius * 0.4f,
                  radius * 0.8f, radius * 0.8f);
}

void SimVPot::mouseDown(const juce::MouseEvent& e)
{
    lastDragY = (int)e.position.y;
}

void SimVPot::mouseDrag(const juce::MouseEvent& e)
{
    int dy = lastDragY - (int)e.position.y;  // up = positive
    lastDragY = (int)e.position.y;

    // Every 4 pixels of drag = 1 tick
    int ticks = dy / 4;
    if (ticks == 0) return;

    Sim::Message m;
    m.type      = Sim::MsgType::VPotTurn;
    m.vpotId    = vpotId;
    m.vpotDelta = ticks;
    bus.publish(m);
}

void SimVPot::setRing(Sim::VPotMode mode, int position)
{
    ringMode     = mode;
    ringPosition = juce::jlimit(0, 11, position);
    repaint();
}
