juce_add_plugin(UberChannelAgent
    VERSION 1.0.0
    COMPANY_NAME "UberController"
    PLUGIN_MANUFACTURER_CODE Uber
    PLUGIN_CODE UbCa
    FORMATS VST3
    PRODUCT_NAME "Uber Channel Agent"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

juce_generate_juce_header(UberChannelAgent)

target_sources(UberChannelAgent PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/ChannelReporter.h
    Source/ChannelReporter.cpp
    Source/MeterSender.h
    Source/MeterSender.cpp
    Source/PslContextBridge.h
    Source/PslContextBridge.cpp
)

target_include_directories(UberChannelAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Need access to VST3 SDK headers for PSL extensions
target_include_directories(UberChannelAgent PRIVATE
    D:/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK
)

target_link_libraries(UberChannelAgent PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_extra
)

target_compile_definitions(UberChannelAgent PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
)
