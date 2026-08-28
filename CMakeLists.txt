cmake_minimum_required(VERSION 3.22)
project(LostAudioEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "JUCE_DIR does not contain CMakeLists.txt: ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

add_subdirectory(camcorder-vst3)
add_subdirectory(cartridge-vst3)
add_subdirectory(cd-vst3)
add_subdirectory(comms-vst3)
add_subdirectory(conference-vst3)
add_subdirectory(occlusion-vst3)
add_subdirectory(openmicnight-vst3)
add_subdirectory(tape-vst3)
add_subdirectory(television-vst3)
add_subdirectory(transmission-vst3)
