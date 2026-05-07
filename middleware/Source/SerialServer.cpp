// SerialServer.cpp
#include "SerialServer.h"

#if JUCE_WINDOWS
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

SerialServer::SerialServer() : juce::Thread("SerialServer") {}

SerialServer::~SerialServer()
{
    stop();
    delete parser;
}

bool SerialServer::start(const std::string& portName, int baud, MessageCallback cb)
{
    onMessage = std::move(cb);
    parser    = new Protocol::FrameParser([this](const Sim::Message& msg)
    {
        if (onMessage) onMessage(msg);
    });

    if (!openPort(portName, baud))
    {
        DBG("SerialServer: failed to open " << portName);
        return false;
    }

    connected = true;
    startThread();
    DBG("SerialServer: opened " << portName << " at " << baud << " baud");
    return true;
}

void SerialServer::stop()
{
    connected = false;
    closePort();
    stopThread(2000);
}

void SerialServer::send(const Sim::Message& msg)
{
    auto frame = Protocol::encode(msg);
    if (frame.empty()) return;
    std::lock_guard<std::mutex> lock(sendMutex);
    sendQueue.push_back(std::move(frame));
}

// ---------------------------------------------------------------------------
// Platform serial I/O
// ---------------------------------------------------------------------------

#if JUCE_WINDOWS

bool SerialServer::openPort(const std::string& portName, int baud)
{
    // Windows requires "\\.\COMx" for ports above COM9
    std::string fullName = "\\\\.\\" + portName;
    portHandle = CreateFileA(fullName.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);

    if (portHandle == INVALID_HANDLE_VALUE)
    {
        portHandle = nullptr;
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    GetCommState((HANDLE)portHandle, &dcb);
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState((HANDLE)portHandle, &dcb);

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = 10;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 10;
    SetCommTimeouts((HANDLE)portHandle, &timeouts);

    return true;
}

void SerialServer::closePort()
{
    if (portHandle)
    {
        CloseHandle((HANDLE)portHandle);
        portHandle = nullptr;
    }
}

void SerialServer::run()
{
    uint8_t buf[256];

    while (!threadShouldExit() && connected)
    {
        // --- Send queued frames ---
        {
            std::lock_guard<std::mutex> lock(sendMutex);
            while (!sendQueue.empty())
            {
                auto& frame = sendQueue.front();
                DWORD written = 0;
                WriteFile((HANDLE)portHandle, frame.data(), (DWORD)frame.size(), &written, nullptr);
                sendQueue.pop_front();
            }
        }

        // --- Read inbound bytes ---
        DWORD bytesRead = 0;
        if (ReadFile((HANDLE)portHandle, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0)
            parser->feed(buf, bytesRead);
    }

    connected = false;
}

#else  // POSIX

bool SerialServer::openPort(const std::string& portName, int baud)
{
    portFd = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (portFd < 0) return false;

    termios tty = {};
    tcgetattr(portFd, &tty);

    speed_t speed = B115200;
    if (baud == 9600)   speed = B9600;
    if (baud == 57600)  speed = B57600;
    if (baud == 115200) speed = B115200;

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 1;
    tcsetattr(portFd, TCSANOW, &tty);
    return true;
}

void SerialServer::closePort()
{
    if (portFd >= 0) { ::close(portFd); portFd = -1; }
}

void SerialServer::run()
{
    uint8_t buf[256];

    while (!threadShouldExit() && connected)
    {
        {
            std::lock_guard<std::mutex> lock(sendMutex);
            while (!sendQueue.empty())
            {
                auto& frame = sendQueue.front();
                ::write(portFd, frame.data(), frame.size());
                sendQueue.pop_front();
            }
        }

        ssize_t n = ::read(portFd, buf, sizeof(buf));
        if (n > 0)
            parser->feed(buf, (size_t)n);
        else
            juce::Thread::sleep(5);
    }

    connected = false;
}

#endif
