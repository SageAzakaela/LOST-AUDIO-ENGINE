cmake_minimum_required(VERSION 3.22)
project(TapeEngineVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(TapeEngine
  COMPANY_NAME "LostAudio"
  BUNDLE_ID "com.lostaudio.tapeengine"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE TpEg
  FORMATS VST3 Standalone
  PRODUCT_NAME "Tape Engine"
)

juce_generate_juce_header(TapeEngine)

juce_add_binary_data(TapeEngineAssets
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Casette_Working_09.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Turn_ON_12.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_New_Cassette_19.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Play_Button_06.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Stop_16.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Casette_End_11.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Turn_OFF_13.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_VHS_Working_05.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_VHS_IN_03.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_Play_Button_17.wav
    ${CMAKE_CURRENT_SOURCE_DIR}/../tape-engine/audio/OASD_VHS_Out_01.wav
)

target_sources(TapeEngine
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(TapeEngine
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(TapeEngine
  PRIVATE
    TapeEngineAssets
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
