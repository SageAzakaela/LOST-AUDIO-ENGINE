cmake_minimum_required(VERSION 3.22)
project(LostAudioSequencerVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure from the repository root or pass -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(LostAudioSequencer
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.bedigital.lostaudiosequencer"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE LaSq
  FORMATS VST3 Standalone
  PRODUCT_NAME "Lost Audio Sequencer"
)

juce_generate_juce_header(LostAudioSequencer)

juce_add_binary_data(LostAudioSequencerAssets
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../suite-vst3/assets/crt-bed.wav
)

target_sources(LostAudioSequencer PRIVATE
  src/PluginEditor.cpp
  src/PluginEditor.h
  src/PluginProcessor.cpp
  src/PluginProcessor.h
)

target_compile_definitions(LostAudioSequencer PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(LostAudioSequencer
  PRIVATE
    LostAudioSequencerAssets
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
  add_executable(lost_audio_sequencer_state_tests tests/ProcessorStateTests.cpp)
  target_include_directories(lost_audio_sequencer_state_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/LostAudioSequencer_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_LostAudioSequencerAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_sequencer_state_tests PRIVATE
    LostAudioSequencer
    LostAudioSequencerAssets
    LostAudio::Core
    juce::juce_audio_plugin_client
    juce::juce_audio_utils
    juce::juce_dsp
  )
  add_test(NAME lost_audio_sequencer_state_tests COMMAND lost_audio_sequencer_state_tests)
endif()
