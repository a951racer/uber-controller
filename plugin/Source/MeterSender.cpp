// MeterSender.cpp
#include "MeterSender.h"
#include <cmath>

MeterSender::MeterSender() {}
MeterSender::~MeterSender() { stop(); }

void MeterSender::start(const juce::String& h, int p)
{
    host = h;
    port = p;
    socket = std::make_unique<juce::DatagramSocket>(false);
    socket->bindToPort(0);  // bind to any available port for sending
    active = true;
}

void MeterSender::stop()
{
    active = false;
    socket.reset();
}

void MeterSender::sendMeter(int channelIndex, float peakL, float peakR)
{
    if (!active || !socket) return;

    // Convert linear gain to 14-bit value using dB scale
    // -60dB = 0, +6dBFS = 16383 (gives headroom above 0dBFS)
    auto linearTo14bit = [](float linear) -> uint16_t
    {
        if (linear <= 0.000001f) return 0;
        float dB = 20.0f * std::log10(linear);
        if (dB < -60.0f) return 0;
        if (dB > 6.0f) dB = 6.0f;
        // Map -60..+6 dB to 0..16383
        float norm = (dB + 60.0f) / 66.0f;
        return static_cast<uint16_t>(norm * 16383.0f);
    };

    uint16_t l = linearTo14bit(peakL);
    uint16_t r = linearTo14bit(peakR);

    // UDP packet: [channelIndex:u8] [peakL_hi:u8] [peakL_lo:u8] [peakR_hi:u8] [peakR_lo:u8]
    uint8_t packet[5];
    packet[0] = static_cast<uint8_t>(channelIndex);
    packet[1] = static_cast<uint8_t>((l >> 7) & 0x7F);
    packet[2] = static_cast<uint8_t>(l & 0x7F);
    packet[3] = static_cast<uint8_t>((r >> 7) & 0x7F);
    packet[4] = static_cast<uint8_t>(r & 0x7F);

    socket->write(host, port, packet, 5);
}
