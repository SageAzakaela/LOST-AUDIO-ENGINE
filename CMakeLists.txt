cmake_minimum_required(VERSION 3.22)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/LostAudioPluginDefaults.cmake)
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

  set(lost_audio_all_projects
    camcorder-vst3 brain-cruncher-vst3 cartridge-vst3 cd-vst3 comms-vst3
    conference-vst3 occlusion-vst3 openmicnight-vst3 sequencer-vst3 suite-vst3
    tape-vst3 television-vst3 transmission-vst3)
  set(LAE_PLUGIN_PROJECTS "${lost_audio_all_projects}" CACHE STRING "Plugin projects to build")
  foreach(plugin_project IN LISTS LAE_PLUGIN_PROJECTS)
    if(NOT plugin_project IN_LIST lost_audio_all_projects)
      message(FATAL_ERROR "Unknown LAE_PLUGIN_PROJECTS entry: ${plugin_project}")
    endif()
    add_subdirectory(${plugin_project})
  endforeach()
endif()
