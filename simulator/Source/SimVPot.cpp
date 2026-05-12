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

    // LED ring: 11 positions spanning 240 degrees
    float startAngle = -120.0f;
    float totalArc   = 240.0f;

    // Use position to determine which LEDs are lit
    int posIdx = static_cast<int>(position * 10.0f + 0.5f);
    posIdx = juce::jlimit(0, 10, posIdx);

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
                lit = (i == posIdx);
                break;
            case Sim::VPotMode::BoostCut:
                lit = (posIdx >= 5) ? (i >= 5 && i <= posIdx)
                                    : (i <= 5 && i >= posIdx);
                break;
            case Sim::VPotMode::Wrap:
                lit = (i <= posIdx);
                break;
            case Sim::VPotMode::Spread:
                lit = (i >= 5 - posIdx && i <= 5 + posIdx);
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
    dragging = true;
}

void SimVPot::mouseDrag(const juce::MouseEvent& e)
{
    int dy = lastDragY - (int)e.position.y;  // up = positive
    lastDragY = (int)e.position.y;

    float delta = dy * 0.005f;
    position = juce::jlimit(0.0f, 1.0f, position + delta);

    Sim::Message m;
    m.type      = Sim::MsgType::VPotTurn;
    m.vpotId    = vpotId;
    m.vpotDelta = 0;        // 0 signals absolute mode
    m.value     = position;

    bus.publish(m);
    repaint();
}

void SimVPot::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
}

void SimVPot::setRing(Sim::VPotMode mode, int pos)
{
    ringMode     = mode;
    ringPosition = juce::jlimit(0, 11, pos);

    // Only update position from external source if not currently dragging
    if (!dragging)
    {
        position = static_cast<float>(pos) / 10.0f;
        repaint();
    }
}

void SimVPot::setPosition(float pos)
{
    if (!dragging)
    {
        position = juce::jlimit(0.0f, 1.0f, pos);
        repaint();
    }
}
