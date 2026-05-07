juce_add_plugin(UberGroupManager
    VERSION 1.0.0
    COMPANY_NAME "UberController"
    PLUGIN_MANUFACTURER_CODE Uber
    PLUGIN_CODE UbGm
    FORMATS VST3
    PRODUCT_NAME "Uber Group Manager"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

juce_generate_juce_header(UberGroupManager)

target_sources(UberGroupManager PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/GroupReporter.h
    Source/GroupReporter.cpp
)

target_include_directories(UberGroupManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(UberGroupManager PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_extra
)

target_compile_definitions(UberGroupManager PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
)
