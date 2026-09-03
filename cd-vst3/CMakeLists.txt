cmake_minimum_required(VERSION 3.22)
project(CDEngineVST3 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(CDEngine
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.lostaudio.cdengine" # Stable legacy identity; publisher is B&E Digital.
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE CdEg
  FORMATS VST3 Standalone
  PRODUCT_NAME "CD Engine"
)

juce_generate_juce_header(CDEngine)

target_sources(CDEngine
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(CDEngine
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(CDEngine
  PRIVATE
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
  add_executable(lost_audio_cd_canonical_state_tests tests/CanonicalStateTests.cpp)
  target_include_directories(lost_audio_cd_canonical_state_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/CDEngine_artefacts/JuceLibraryCode
  )
  target_link_libraries(lost_audio_cd_canonical_state_tests PRIVATE CDEngine LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME lost_audio_cd_canonical_state_tests COMMAND lost_audio_cd_canonical_state_tests)
endif()
