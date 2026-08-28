cmake_minimum_required(VERSION 3.22)
project(LostAudioEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LAE_BUILD_PLUGINS "Build the JUCE plugin fleet" ON)
include(CTest)
add_subdirectory(native/core)

if(LAE_BUILD_PLUGINS)
  if(DEFINED JUCE_DIR)
    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "JUCE_DIR does not contain CMakeLists.txt: ${JUCE_DIR}")
    endif()
    add_subdirectory("${JUCE_DIR}" JUCE)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8
    )
    FetchContent_MakeAvailable(JUCE)
    FetchContent_GetProperties(JUCE SOURCE_DIR juce_source_directory)
    set(JUCE_DIR "${juce_source_directory}" CACHE PATH "Pinned JUCE source directory" FORCE)
  endif()

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
endif()
