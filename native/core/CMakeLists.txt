add_library(lost_audio_core STATIC
  src/SuiteProcessor.cpp
  src/OpenMicProcessor.cpp
  src/OcclusionProcessor.cpp
  src/TelevisionProcessor.cpp
  src/CamcorderProcessor.cpp
  src/CartridgeProcessor.cpp
  src/CDProcessor.cpp
  src/CommsProcessor.cpp
  src/ConferenceProcessor.cpp
  src/TapeProcessor.cpp
  src/TransmissionProcessor.cpp
)

add_library(LostAudio::Core ALIAS lost_audio_core)

target_include_directories(lost_audio_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(lost_audio_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(lost_audio_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(lost_audio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(lost_audio_suite_core_tests tests/SuiteProcessorTests.cpp)
  target_link_libraries(lost_audio_suite_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_suite_core_tests COMMAND lost_audio_suite_core_tests)

  add_executable(lost_audio_open_mic_core_tests tests/OpenMicProcessorTests.cpp)
  target_link_libraries(lost_audio_open_mic_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_open_mic_core_tests COMMAND lost_audio_open_mic_core_tests)

  add_executable(lost_audio_occlusion_core_tests tests/OcclusionProcessorTests.cpp)
  target_link_libraries(lost_audio_occlusion_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_occlusion_core_tests COMMAND lost_audio_occlusion_core_tests)

  add_executable(lost_audio_television_core_tests tests/TelevisionProcessorTests.cpp)
  target_link_libraries(lost_audio_television_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_television_core_tests COMMAND lost_audio_television_core_tests)

  add_executable(lost_audio_cartridge_core_tests tests/CartridgeProcessorTests.cpp)
  target_link_libraries(lost_audio_cartridge_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_cartridge_core_tests COMMAND lost_audio_cartridge_core_tests)

  add_executable(lost_audio_camcorder_core_tests
    tests/CamcorderProcessorTests.cpp
  )
  target_link_libraries(lost_audio_camcorder_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_camcorder_core_tests COMMAND lost_audio_camcorder_core_tests)

  add_executable(lost_audio_core_tests
    tests/TapeProcessorTests.cpp
  )
  target_link_libraries(lost_audio_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_core_tests COMMAND lost_audio_core_tests)

  add_executable(lost_audio_transmission_core_tests
    tests/TransmissionProcessorTests.cpp
  )
  target_link_libraries(lost_audio_transmission_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_transmission_core_tests COMMAND lost_audio_transmission_core_tests)

  add_executable(lost_audio_comms_core_tests
    tests/CommsProcessorTests.cpp
  )
  target_link_libraries(lost_audio_comms_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_comms_core_tests COMMAND lost_audio_comms_core_tests)

  add_executable(lost_audio_conference_core_tests
    tests/ConferenceProcessorTests.cpp
  )
  target_link_libraries(lost_audio_conference_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_conference_core_tests COMMAND lost_audio_conference_core_tests)

  add_executable(lost_audio_cd_core_tests
    tests/CDProcessorTests.cpp
  )
  target_link_libraries(lost_audio_cd_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_cd_core_tests COMMAND lost_audio_cd_core_tests)

  add_executable(lost_audio_tempo_sync_tests
    tests/TempoSyncTests.cpp
  )
  target_link_libraries(lost_audio_tempo_sync_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_tempo_sync_tests COMMAND lost_audio_tempo_sync_tests)

  add_executable(lost_audio_sequencer_clock_tests
    tests/SequencerClockTests.cpp
  )
  target_link_libraries(lost_audio_sequencer_clock_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_sequencer_clock_tests COMMAND lost_audio_sequencer_clock_tests)

  add_executable(lost_audio_fleet_click_diagnostics tests/FleetClickDiagnostics.cpp)
  target_link_libraries(lost_audio_fleet_click_diagnostics PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_fleet_click_diagnostics COMMAND lost_audio_fleet_click_diagnostics)
endif()
