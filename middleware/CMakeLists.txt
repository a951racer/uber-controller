juce_add_console_app(McuMiddleware
    VERSION 1.0.0
    PRODUCT_NAME "McuMiddleware")

juce_generate_juce_header(McuMiddleware)

target_sources(McuMiddleware PRIVATE
    Source/Main.cpp
    Source/Config.h
    Source/Config.cpp
    Source/McuProtocol.h
    Source/MidiEngine.h
    Source/MidiEngine.cpp
    Source/TcpServer.h
    Source/TcpServer.cpp
    Source/SerialServer.h
    Source/SerialServer.cpp
    Source/PluginServer.h
    Source/PluginServer.cpp
    Source/TrackRegistry.h
    Source/TrackRegistry.cpp
    Source/ClientConnection.h
    Source/MiddlewareApp.h
    Source/MiddlewareApp.cpp
)

target_include_directories(McuMiddleware PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(McuMiddleware PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_devices
)
