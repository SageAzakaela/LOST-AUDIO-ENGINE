cmake_minimum_required(VERSION 3.22)
project(OpenMicNightVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(OpenMicNight
  COMPANY_NAME "LostAudio"
  BUNDLE_ID "com.lostaudio.openmicnight"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE OmNt
  FORMATS VST3 Standalone
  PRODUCT_NAME "Open Mic Night"
)

juce_generate_juce_header(OpenMicNight)

juce_add_binary_data(OpenMicNightAssets
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../openmicnight/audio/freesound_community-city-terrace-night-16943.mp3
    ${CMAKE_CURRENT_SOURCE_DIR}/../openmicnight/audio/freesound_community-new-york-city-night-evening-rooftop-ambience-traffic-bushwick-brooklyn-above-ground-subway-m-train-6775.mp3
    ${CMAKE_CURRENT_SOURCE_DIR}/../openmicnight/audio/dbsound-rain-falling-at-night-cars-and-people-in-the-distance-248232.mp3
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/banter.mp3
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/bandmate_intro.mp3
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/applause.mp3
)

target_sources(OpenMicNight
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(OpenMicNight
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(OpenMicNight
  PRIVATE
    OpenMicNightAssets
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
