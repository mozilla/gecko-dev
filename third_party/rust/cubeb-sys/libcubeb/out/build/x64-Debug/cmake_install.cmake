# Install script for directory: C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug")
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

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/include/cubeb")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/cubeb" TYPE DIRECTORY FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/exports/")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/cubeb.lib")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/cubebConfig.cmake"
    "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/cubebConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb/cubebTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb/cubebTargets.cmake"
         "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/CMakeFiles/Export/lib/cmake/cubeb/cubebTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb/cubebTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb/cubebTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb" TYPE FILE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/CMakeFiles/Export/lib/cmake/cubeb/cubebTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/cubeb" TYPE FILE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/CMakeFiles/Export/lib/cmake/cubeb/cubebTargets-debug.cmake")
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_sanity.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_sanity.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_tone.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_tone.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_audio.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_audio.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_record.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_record.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_devices.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_devices.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_callback_ret.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_callback_ret.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_resampler.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_resampler.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_duplex.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_duplex.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_overload_callback.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_overload_callback.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_loopback.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_loopback.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_latency.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_latency.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_ring_array.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_ring_array.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_utils.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_utils.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_ring_buffer.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_ring_buffer.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/test_device_changed_callback.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/test_device_changed_callback.exe")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin/cubeb-test.exe")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/install/x64-Debug/bin" TYPE EXECUTABLE FILES "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/cubeb-test.exe")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/googletest/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/mozilla-source/gecko-dev/third_party/rust/cubeb-sys/libcubeb/out/build/x64-Debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
