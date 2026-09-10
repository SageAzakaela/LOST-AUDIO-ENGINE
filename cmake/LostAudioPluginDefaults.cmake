# Include before project() so CMake configures both Mac architectures and the
# deployment target before probing the compiler. Explicit overrides are kept.
if(CMAKE_HOST_APPLE AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac plugin architectures")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest targeted macOS version")
  set(LAE_PLUGIN_FORMATS "VST3;AU;Standalone" CACHE STRING "JUCE plugin formats")
else()
  set(LAE_PLUGIN_FORMATS "VST3;Standalone" CACHE STRING "JUCE plugin formats")
endif()

if(NOT LAE_PLUGIN_FORMATS)
  message(FATAL_ERROR "LAE_PLUGIN_FORMATS must contain at least one plugin format")
endif()
