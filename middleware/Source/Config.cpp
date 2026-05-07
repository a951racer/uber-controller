// Config.cpp
#include "Config.h"
#include <JuceHeader.h>

Config Config::defaults()
{
    Config cfg;
    cfg.mcuUnits.push_back({ "", "" });
    return cfg;
}

Config Config::loadFromFile(const std::string& path)
{
    Config cfg;

    juce::File f(path);
    if (!f.existsAsFile())
        return defaults();

    auto json = juce::JSON::parse(f.loadFileAsString());
    if (!json.isObject())
        return defaults();

    auto* obj = json.getDynamicObject();
    if (!obj) return defaults();

    // Check for new multi-unit format
    if (obj->hasProperty("mcuUnits"))
    {
        auto* units = obj->getProperty("mcuUnits").getArray();
        if (units)
        {
            for (auto& unitVar : *units)
            {
                auto* unitObj = unitVar.getDynamicObject();
                if (unitObj)
                {
                    McuUnitConfig unit;
                    unit.midiIn  = unitObj->getProperty("midiIn").toString().toStdString();
                    unit.midiOut = unitObj->getProperty("midiOut").toString().toStdString();
                    cfg.mcuUnits.push_back(unit);
                }
            }
        }
    }
    else
    {
        // Legacy single-unit format
        McuUnitConfig unit;
        if (obj->hasProperty("midiIn"))
            unit.midiIn = obj->getProperty("midiIn").toString().toStdString();
        if (obj->hasProperty("midiOut"))
            unit.midiOut = obj->getProperty("midiOut").toString().toStdString();
        cfg.mcuUnits.push_back(unit);
    }

    if (obj->hasProperty("tcpPort"))
        cfg.tcpPort = static_cast<int>(obj->getProperty("tcpPort"));

    if (obj->hasProperty("serialPort"))
        cfg.serialPort = obj->getProperty("serialPort").toString().toStdString();

    if (obj->hasProperty("serialBaud"))
        cfg.serialBaud = static_cast<int>(obj->getProperty("serialBaud"));

    return cfg;
}
