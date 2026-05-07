cmake_minimum_required(VERSION 3.22)

project(UberController VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# ---- Point this at your local JUCE checkout --------------------------------
add_subdirectory(D:/JUCE JUCE)
# ----------------------------------------------------------------------------

add_subdirectory(simulator)
add_subdirectory(middleware)
add_subdirectory(plugin)
