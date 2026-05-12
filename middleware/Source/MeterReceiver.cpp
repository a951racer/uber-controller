// MeterReceiver.cpp
#include "MeterReceiver.h"
#include <iostream>

#if JUCE_WINDOWS
  #pragma comment(lib, "ws2_32.lib")
  #define INVALID_SOCK INVALID_SOCKET
#else
  #include <unistd.h>
  #define INVALID_SOCK (-1)
#endif

MeterReceiver::MeterReceiver() {}
MeterReceiver::~MeterReceiver() { stop(); }

bool MeterReceiver::start(int port, MeterCallback cb)
{
    onMeter = std::move(cb);

#if JUCE_WINDOWS
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCK) return false;

    sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0)
    {
#if JUCE_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    // Set receive timeout so the thread can check `running`
#if JUCE_WINDOWS
    DWORD timeout = 200;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = { 0, 200000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    running = true;
    recvThread = std::thread([this]() { receiveLoop(); });

    std::cout << "[Meters] Listening on UDP port " << port << std::endl;
    return true;
}

void MeterReceiver::stop()
{
    running = false;
    if (sock != 0 && sock != INVALID_SOCK)
    {
#if JUCE_WINDOWS
        closesocket(sock);
#else
        close(sock);
#endif
        sock = 0;
    }
    if (recvThread.joinable()) recvThread.join();
}

void MeterReceiver::receiveLoop()
{
    uint8_t buf[64];

    while (running)
    {
        int n = recvfrom(sock, (char*)buf, sizeof(buf), 0, nullptr, nullptr);

        if (n == 5)
        {
            // [channelIndex:u8] [peakL_hi:u8] [peakL_lo:u8] [peakR_hi:u8] [peakR_lo:u8]
            Sim::Message msg;
            msg.type         = Sim::MsgType::MeterUpdate;
            msg.meterChannel = buf[0];
            msg.meterPeakL   = ((buf[1] & 0x7F) << 7) | (buf[2] & 0x7F);
            msg.meterPeakR   = ((buf[3] & 0x7F) << 7) | (buf[4] & 0x7F);

            if (onMeter)
                onMeter(msg);
        }
        // Ignore malformed packets
    }
}
