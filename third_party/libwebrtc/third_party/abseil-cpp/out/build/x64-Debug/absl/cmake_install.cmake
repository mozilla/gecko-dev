# Install script for directory: C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/absl

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/base/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/algorithm/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/container/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/debugging/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/flags/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/functional/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/hash/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/memory/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/meta/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/numeric/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/random/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/status/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/strings/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/synchronization/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/time/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/types/cmake_install.cmake")
  include("C:/mozilla-source/gecko-dev/third_party/libwebrtc/third_party/abseil-cpp/out/build/x64-Debug/absl/utility/cmake_install.cmake")

endif()

