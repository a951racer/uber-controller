// Main.cpp  (middleware)
#include <JuceHeader.h>
#include "MiddlewareApp.h"
#include "Config.h"
#include <iostream>

int main(int /*argc*/, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory();
    std::string configPath = exeDir.getChildFile("middleware.json").getFullPathName().toStdString();

    Config cfg = Config::loadFromFile(configPath);

    std::cout << "=== MCU Middleware ===" << std::endl;
    std::cout << "Config:      " << configPath << std::endl;
    std::cout << "MCU Units:   " << cfg.mcuUnits.size() << " (" << cfg.totalChannels() << " channels)" << std::endl;
    for (int i = 0; i < (int)cfg.mcuUnits.size(); ++i)
    {
        std::cout << "  Unit " << i << ": In=\"" << cfg.mcuUnits[i].midiIn
                  << "\" Out=\"" << cfg.mcuUnits[i].midiOut << "\"" << std::endl;
    }
    std::cout << "TCP port:    " << cfg.tcpPort << std::endl;
    std::cout << "Plugin port: 9001" << std::endl;
    std::cout << "Serial:      " << (cfg.serialPort.empty() ? "(disabled)" : cfg.serialPort) << std::endl;

    MiddlewareApp app;
    app.start(cfg);

    std::cout << "Middleware running. Press Enter to quit." << std::endl;
    std::cin.get();

    app.stop();
    return 0;
}
