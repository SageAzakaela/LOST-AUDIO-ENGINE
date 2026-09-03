cmake_minimum_required(VERSION 3.22)
project(CamcorderEngineVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
  message(FATAL_ERROR "JUCE_DIR is not set. Configure with -DJUCE_DIR=<path-to-JUCE>")
endif()

if(NOT TARGET juce::juce_audio_utils)
  add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(CamcorderEngine
  COMPANY_NAME "B&E Digital"
  BUNDLE_ID "com.lostaudio.camcorderengine" # Stable legacy identity; publisher is B&E Digital.
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE LsAu
  PLUGIN_CODE CcEg
  FORMATS VST3 Standalone
  PRODUCT_NAME "Camcorder Engine"
)

juce_generate_juce_header(CamcorderEngine)

juce_add_binary_data(CamcorderEngineAssets
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/camera-bed-1.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/camera-bed-2.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/camera-bed-3.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/camera-bed-4.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/wind-bed-1.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/wind-bed-2.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/wind-bed-3.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/wind-bed-4.wav"
)

target_sources(CamcorderEngine
  PRIVATE
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
)

target_compile_definitions(CamcorderEngine
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
)

target_link_libraries(CamcorderEngine
  PRIVATE
    CamcorderEngineAssets
    LostAudio::Core
    juce::juce_dsp
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
  add_executable(lost_audio_camcorder_adapter_tests tests/BedAdapterTests.cpp)
  target_include_directories(lost_audio_camcorder_adapter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/CamcorderEngine_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_CamcorderEngineAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_camcorder_adapter_tests PRIVATE CamcorderEngine CamcorderEngineAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME lost_audio_camcorder_adapter_tests COMMAND lost_audio_camcorder_adapter_tests)

  add_executable(lost_audio_camcorder_visual_harness tests/VisualHarness.cpp)
  target_include_directories(lost_audio_camcorder_visual_harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/CamcorderEngine_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_CamcorderEngineAssets/JuceLibraryCode
  )
  target_link_libraries(lost_audio_camcorder_visual_harness PRIVATE CamcorderEngine CamcorderEngineAssets LostAudio::Core juce::juce_audio_plugin_client juce::juce_audio_utils juce::juce_dsp)
endif()
