cmake_minimum_required(VERSION 3.22)
project(TelevisionEngineVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(TelevisionEngine
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.lostaudio.televisionengine" # Stable legacy identity; publisher is B&E Digital.
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE TvEg
  # Windows Application Control blocks JUCE's newly generated helper in local developer builds.
  VST3_AUTO_MANIFEST FALSE
  FORMATS VST3 Standalone
  PRODUCT_NAME "Television Engine"
)

juce_generate_juce_header(TelevisionEngine)

juce_add_binary_data(TelevisionEngineAssets
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/crt-bed.wav
)

target_sources(TelevisionEngine
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(TelevisionEngine
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(TelevisionEngine
  PRIVATE
    TelevisionEngineAssets
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
  add_executable(lost_audio_television_adapter_tests tests/BedAdapterTests.cpp)
  target_include_directories(lost_audio_television_adapter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/TelevisionEngine_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_TelevisionEngineAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_television_adapter_tests PRIVATE TelevisionEngine TelevisionEngineAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME lost_audio_television_adapter_tests COMMAND lost_audio_television_adapter_tests)
  add_executable(lost_audio_television_canonical_tests tests/CanonicalStateTests.cpp)
  target_include_directories(lost_audio_television_canonical_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/TelevisionEngine_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_TelevisionEngineAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_television_canonical_tests PRIVATE TelevisionEngine TelevisionEngineAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME lost_audio_television_canonical_tests COMMAND lost_audio_television_canonical_tests)
endif()
