// SerialServer.h
// Manages a serial port connection to hardware (USB CDC).
// Uses JUCE's SerialPort via the juce_serialport module, or falls back to
// platform APIs if that module is unavailable.
// For now implemented using raw Win32 / POSIX serial so there's no extra
// JUCE module dependency.
#pragma once
#include <JuceHeader.h>
#include "ClientConnection.h"
#include <functional>
#include <atomic>
#include <deque>
#include <mutex>

class SerialServer : private juce::Thread
{
public:
    using MessageCallback = std::function<void(const Sim::Message&)>;

    SerialServer();
    ~SerialServer() override;

    /** Open the serial port and start reading.
        portName: e.g. "COM3" on Windows, "/dev/ttyUSB0" on Linux.
        baud: baud rate (default 115200). */
    bool start(const std::string& portName, int baud, MessageCallback cb);
    void stop();

    bool isConnected() const { return connected.load(); }

    /** Send a message to the hardware. */
    void send(const Sim::Message& msg);

private:
    void run() override;
    bool openPort(const std::string& portName, int baud);
    void closePort();

    MessageCallback   onMessage;
    Protocol::FrameParser* parser = nullptr;

    std::atomic<bool> connected { false };

    std::mutex              sendMutex;
    std::deque<std::vector<uint8_t>> sendQueue;

#if JUCE_WINDOWS
    void* portHandle = nullptr;   // HANDLE
#else
    int   portFd = -1;
#endif
};
