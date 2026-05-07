juce_add_gui_app(SimSurface
    VERSION 1.0.0
    PRODUCT_NAME "SimSurface")

juce_generate_juce_header(SimSurface)

target_sources(SimSurface PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/MessageBus.h
    Source/ControllerClient.h
    Source/ControllerClient.cpp
    Source/SimFader.h
    Source/SimFader.cpp
    Source/SimVPot.h
    Source/SimVPot.cpp
    Source/SimTransport.h
    Source/SimTransport.cpp
    Source/SimChannelStrip.h
    Source/SimChannelStrip.cpp
    Source/DebugPanel.h
    Source/DebugPanel.cpp
)

target_include_directories(SimSurface PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(SimSurface PRIVATE
    juce::juce_gui_extra
)
