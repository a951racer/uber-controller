// MeterSender.h
// Sends meter levels via UDP to the middleware.
// Fire-and-forget — no connection state, no acknowledgment.
#pragma once
#include <JuceHeader.h>
#include <atomic>

class MeterSender
{
public:
    MeterSender();
    ~MeterSender();

    void start(const juce::String& host, int port);
    void stop();

    /** Send peak levels for a channel. Called from processBlock. 
        peakL/peakR are linear gain values (0.0 to 1.0+). */
    void sendMeter(int channelIndex, float peakL, float peakR);

private:
    std::unique_ptr<juce::DatagramSocket> socket;
    juce::String host;
    int port = 9002;
    std::atomic<bool> active { false };
};
