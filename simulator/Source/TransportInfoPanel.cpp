// TransportInfoPanel.cpp
#include "TransportInfoPanel.h"

TransportInfoPanel::TransportInfoPanel() {}

void TransportInfoPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111));

    auto bounds = getLocalBounds().reduced(4, 2);

    // Left side: Time (HH:MM:SS.mmm)
    auto leftArea = bounds.removeFromLeft(bounds.getWidth() / 3);
    {
        // Compute time from samples
        double seconds = (sampleRate > 0) ? (double)samples / sampleRate : 0.0;
        int hrs  = (int)(seconds / 3600.0);
        int mins = (int)(std::fmod(seconds, 3600.0) / 60.0);
        int secs = (int)(std::fmod(seconds, 60.0));
        int ms   = (int)(std::fmod(seconds, 1.0) * 1000.0);

        juce::String timeStr = juce::String::formatted("%02d:%02d:%02d.%03d", hrs, mins, secs, ms);

        g.setColour(juce::Colour(0xffdddddd));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain));
        g.drawText(timeStr, leftArea.removeFromTop(28), juce::Justification::centred);

        g.setColour(juce::Colour(0xff888888));
        g.setFont(10.0f);
        g.drawText("Seconds", leftArea, juce::Justification::centredTop);
    }

    // Center: Bars (BBBBB.BB.TT.tt)
    auto centerArea = bounds.removeFromLeft(bounds.getWidth() / 2);
    {
        // Format: bar.beat.tick (tick from fractional PPQ)
        double beatFrac = std::fmod(ppq, 1.0);
        int ticks = (int)(beatFrac * 100.0);

        juce::String barStr = juce::String::formatted("%05d.%02d.%02d", bar, beat, ticks);

        g.setColour(juce::Colour(0xffdddddd));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain));
        g.drawText(barStr, centerArea.removeFromTop(28), juce::Justification::centred);

        g.setColour(juce::Colour(0xff888888));
        g.setFont(10.0f);
        g.drawText("Bars", centerArea, juce::Justification::centredTop);
    }

    // Right: BPM, Time Sig, Status, Loop
    auto rightArea = bounds;
    {
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));

        auto row1 = rightArea.removeFromTop(14);
        auto row2 = rightArea.removeFromTop(14);
        auto row3 = rightArea.removeFromTop(14);

        // BPM + Time Sig
        g.setColour(juce::Colours::white);
        g.drawText(juce::String(bpm, 1) + " BPM  " + juce::String(timeSigN) + "/" + juce::String(timeSigD),
                   row1, juce::Justification::centredLeft);

        // Playing status
        g.setColour(playing ? juce::Colours::green : juce::Colours::grey);
        g.drawText(playing ? "PLAYING" : "STOPPED", row2, juce::Justification::centredLeft);

        // Loop
        g.setColour(looping ? juce::Colour(0xff44aaff) : juce::Colours::grey);
        juce::String loopStr = looping ? ("Loop: " + juce::String(loopStart, 1) + "-" + juce::String(loopEnd, 1))
                                       : "Loop: OFF";
        g.drawText(loopStr, row3, juce::Justification::centredLeft);
    }
}

void TransportInfoPanel::update(const Sim::Message& msg)
{
    bpm        = msg.transportBpm;
    timeSigN   = msg.transportTimeSigN;
    timeSigD   = msg.transportTimeSigD;
    bar        = msg.transportBar;
    beat       = msg.transportBeat;
    ppq        = msg.transportPpq;
    samples    = msg.transportSamples;
    sampleRate = msg.transportSampleRate;
    playing    = msg.transportPlaying;
    looping    = msg.transportLooping;
    loopStart  = msg.transportLoopStart;
    loopEnd    = msg.transportLoopEnd;
    repaint();
}
