// SimVcaStrip.cpp
#include "SimVcaStrip.h"

SimVcaStrip::SimVcaStrip(int id, const juce::String& name, Sim::MessageBus& b)
    : groupId(id), groupName(name), bus(b)
{
}

void SimVcaStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Name label at top
    auto nameArea = bounds.removeFromTop(24);
    g.setColour(juce::Colour(0xff3d2b1f));  // dark brown header
    g.fillRect(nameArea);
    g.setColour(juce::Colour(0xffddaa66));  // warm gold text
    g.setFont(10.0f);
    g.drawText(groupName, nameArea.reduced(2, 0), juce::Justification::centred);

    // Fader area
    g.setColour(juce::Colour(0xff4a3728));  // brown fader background
    g.fillRect(bounds);

    // Center line (unity mark)
    int centerY = bounds.getY() + bounds.getHeight() / 2;
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(centerY, (float)bounds.getX(), (float)bounds.getRight());

    // Fader knob
    int faderY = bounds.getY() + (int)((1.0f - value) * bounds.getHeight());
    g.setColour(juce::Colour(0xffddaa66));  // warm gold knob
    g.fillRect(bounds.getX() + 2, faderY - 3, bounds.getWidth() - 4, 6);
}

void SimVcaStrip::resized() {}

void SimVcaStrip::mouseDown(const juce::MouseEvent& e)
{
    auto faderArea = getLocalBounds().withTrimmedTop(24);
    value = yToValue((int)e.position.y - faderArea.getY());

    Sim::Message m;
    m.type       = Sim::MsgType::VcaFaderMove;
    m.vcaGroupId = groupId;
    m.vcaValue   = value;
    bus.publish(m);
    repaint();
}

void SimVcaStrip::mouseDrag(const juce::MouseEvent& e)
{
    auto faderArea = getLocalBounds().withTrimmedTop(24);
    value = yToValue((int)e.position.y - faderArea.getY());

    Sim::Message m;
    m.type       = Sim::MsgType::VcaFaderMove;
    m.vcaGroupId = groupId;
    m.vcaValue   = value;
    bus.publish(m);
    repaint();
}

void SimVcaStrip::setGroupName(const juce::String& name)
{
    groupName = name;
    repaint();
}

void SimVcaStrip::setFaderValue(float v)
{
    value = juce::jlimit(0.0f, 1.0f, v);
    repaint();
}

float SimVcaStrip::yToValue(int y)
{
    auto faderArea = getLocalBounds().withTrimmedTop(24);
    float h = (float)faderArea.getHeight();
    return 1.0f - juce::jlimit(0.0f, h, (float)y) / h;
}
