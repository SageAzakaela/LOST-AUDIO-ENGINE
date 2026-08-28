add_library(lost_audio_core STATIC
  src/TapeProcessor.cpp
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
  add_executable(lost_audio_core_tests
    tests/TapeProcessorTests.cpp
  )
  target_link_libraries(lost_audio_core_tests PRIVATE LostAudio::Core)
  add_test(NAME lost_audio_core_tests COMMAND lost_audio_core_tests)
endif()
