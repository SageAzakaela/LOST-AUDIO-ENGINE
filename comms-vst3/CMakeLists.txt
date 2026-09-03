cmake_minimum_required(VERSION 3.22)
project(CommsEngineVST3 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

if(NOT TARGET lost_audio_core)
  include(CTest)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../native/core ${CMAKE_CURRENT_BINARY_DIR}/lost-audio-core)
endif()

juce_add_plugin(CommsEngine
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.lostaudio.commsengine" # Stable legacy identity; publisher is B&E Digital.
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE CmEg
  FORMATS VST3 Standalone
  PRODUCT_NAME "Comms Engine"
)

juce_generate_juce_header(CommsEngine)

target_sources(CommsEngine
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(CommsEngine
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(CommsEngine
  PRIVATE
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
