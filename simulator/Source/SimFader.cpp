// SimFader.cpp
#include "SimFader.h"

SimFader::SimFader(int id, Sim::MessageBus& b)
    : faderId(id), bus(b)
{
}

void SimFader::paint(juce::Graphics& g)
{
    auto r = getLocalBounds();

    g.fillAll(juce::Colours::darkgrey);

    int faderY = (int)((1.0f - value) * r.getHeight());

    g.setColour(juce::Colours::white);
    g.fillRect(r.withTop(faderY).withHeight(6));

    g.setColour(juce::Colours::black);
    g.drawText(juce::String(faderId + 1), r, juce::Justification::centredBottom);
}

void SimFader::mouseDown(const juce::MouseEvent&)
{
    touched = true;
    Sim::Message m;
    m.type    = Sim::MsgType::FaderTouch;
    m.faderId = faderId;
    m.value   = value;
    m.touched = true;
    bus.publish(m);
}

void SimFader::mouseDrag(const juce::MouseEvent& e)
{
    value = yToValue((int)e.position.y);
    Sim::Message m;
    m.type    = Sim::MsgType::FaderMove;
    m.faderId = faderId;
    m.value   = value;
    m.touched = true;
    bus.publish(m);
    repaint();
}

void SimFader::mouseUp(const juce::MouseEvent&)
{
    touched = false;
    Sim::Message m;
    m.type    = Sim::MsgType::FaderTouch;
    m.faderId = faderId;
    m.value   = value;
    m.touched = false;
    bus.publish(m);
}

void SimFader::setValue(float v)
{
    value = juce::jlimit(0.0f, 1.0f, v);
    repaint();
}

float SimFader::yToValue(int y)
{
    return 1.0f - juce::jlimit(0.0f, (float)getHeight(), (float)y) / (float)getHeight();
}
