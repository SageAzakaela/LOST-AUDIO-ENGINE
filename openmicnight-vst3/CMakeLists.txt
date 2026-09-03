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
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.lostaudio.openmicnight" # Stable legacy identity; publisher is B&E Digital.
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE OmNt
  FORMATS VST3 Standalone
  PRODUCT_NAME "Open Mic Night"
)

juce_generate_juce_header(OpenMicNight)

juce_add_binary_data(OpenMicNightAssets
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/applause.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/reaction-cheer.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient-quiet.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient-banter.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient-busy.mp3"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient-rowdy.mp3"
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
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
  add_executable(lost_audio_open_mic_adapter_tests tests/BedAdapterTests.cpp)
  target_include_directories(lost_audio_open_mic_adapter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/OpenMicNight_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_OpenMicNightAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_open_mic_adapter_tests PRIVATE OpenMicNight OpenMicNightAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME lost_audio_open_mic_adapter_tests COMMAND lost_audio_open_mic_adapter_tests)

  add_executable(lost_audio_open_mic_visual_harness tests/VisualHarness.cpp)
  target_include_directories(lost_audio_open_mic_visual_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/OpenMicNight_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_OpenMicNightAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_open_mic_visual_harness PRIVATE OpenMicNight OpenMicNightAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
endif()
