// Config.h
// Loads middleware.json from the same directory as the executable.
//
// middleware.json example:
// {
//   "mcuUnits": [
//     { "midiIn": "MCU_In", "midiOut": "MCU_Out" },
//     { "midiIn": "MCU_In2", "midiOut": "MCU_Out2" },
//     { "midiIn": "MCU_In3", "midiOut": "MCU_Out3" }
//   ],
//   "tcpPort": 8888,
//   "serialPort": "COM3",
//   "serialBaud": 115200
// }
//
// Legacy single-unit format is still supported:
// { "midiIn": "MCU_In", "midiOut": "MCU_Out", ... }
//
#pragma once
#include <string>
#include <vector>

struct McuUnitConfig
{
    std::string midiIn;
    std::string midiOut;
};

struct Config
{
    std::vector<McuUnitConfig> mcuUnits;  // 1-3 MCU units
    int         tcpPort    = 8888;
    std::string serialPort;
    int         serialBaud = 115200;

    int totalChannels() const { return (int)mcuUnits.size() * 8; }

    static Config loadFromFile(const std::string& path);
    static Config defaults();
};
