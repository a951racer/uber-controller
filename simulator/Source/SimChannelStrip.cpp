// SimChannelStrip.cpp
#include "SimChannelStrip.h"

SimChannelStrip::SimChannelStrip(int ch, Sim::MessageBus& b)
    : channelId(ch),
      bus(b),
      fader(ch, b),
      vpot(ch, b),
      recBtn   (static_cast<int>(Sim::ChannelButton::Rec)    + ch, "R", juce::Colour(0xffcc2020), b),
      soloBtn  (static_cast<int>(Sim::ChannelButton::Solo)   + ch, "S", juce::Colour(0xffcccc00), b),
      muteBtn  (static_cast<int>(Sim::ChannelButton::Mute)   + ch, "M", juce::Colour(0xffcc6600), b),
      selectBtn(static_cast<int>(Sim::ChannelButton::Select) + ch, "Sel", juce::Colour(0xff00aacc), b)
{
    addAndMakeVisible(fader);
    addAndMakeVisible(vpot);
    addAndMakeVisible(recBtn);
    addAndMakeVisible(soloBtn);
    addAndMakeVisible(muteBtn);
    addAndMakeVisible(selectBtn);
}

void SimChannelStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // LCD area at top
    auto lcdArea = bounds.removeFromTop(30);
    g.setColour(juce::Colour(0xff001800));
    g.fillRect(lcdArea);

    g.setColour(juce::Colour(0xff00cc44));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));

    auto topRow = lcdArea.removeFromTop(15);
    auto botRow = lcdArea;

    // Use plugin name for top row if available, otherwise MCU LCD text
    juce::String displayRow1 = pluginName.isNotEmpty() ? pluginName : lcdRow1;
    g.drawText(displayRow1, topRow.reduced(2, 0), juce::Justification::centredLeft);
    g.drawText(lcdRow2, botRow.reduced(2, 0), juce::Justification::centredLeft);
}

void SimChannelStrip::resized()
{
    auto bounds = getLocalBounds();

    // LCD: top 30px (painted in paint())
    bounds.removeFromTop(30);

    // VPot: 50px
    vpot.setBounds(bounds.removeFromTop(50));

    // Buttons: 4 rows × 20px
    recBtn   .setBounds(bounds.removeFromTop(20).reduced(2, 1));
    soloBtn  .setBounds(bounds.removeFromTop(20).reduced(2, 1));
    muteBtn  .setBounds(bounds.removeFromTop(20).reduced(2, 1));
    selectBtn.setBounds(bounds.removeFromTop(20).reduced(2, 1));

    // Fader: remaining space
    bounds.removeFromTop(4);
    fader.setBounds(bounds);
}

void SimChannelStrip::setFaderValue(float v)
{
    fader.setValue(v);
}

void SimChannelStrip::setVPotRing(Sim::VPotMode mode, int position)
{
    vpot.setRing(mode, position);
}

void SimChannelStrip::setButtonLed(int noteNumber, bool on)
{
    if (noteNumber == recBtn.noteNumber)    { recBtn.ledOn    = on; recBtn.repaint();    }
    if (noteNumber == soloBtn.noteNumber)   { soloBtn.ledOn   = on; soloBtn.repaint();   }
    if (noteNumber == muteBtn.noteNumber)   { muteBtn.ledOn   = on; muteBtn.repaint();   }
    if (noteNumber == selectBtn.noteNumber) { selectBtn.ledOn = on; selectBtn.repaint(); }
}

void SimChannelStrip::setLcdText(int row, const juce::String& text)
{
    if (row == 0)
        lcdRow1 = text;
    else
        lcdRow2 = text;
    repaint();
}
void SimChannelStrip::setPluginName(const juce::String& name)
{
    pluginName = name;
    repaint();
}
