# Copyright (c) the JPEG XL Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

include(jxl_lists.cmake)

if(SANITIZER STREQUAL "msan")
  message(STATUS "NOT building benchmarks under MSAN")
  return()
endif()

# This is the Google benchmark project (https://github.com/google/benchmark).
find_package(benchmark)

if(benchmark_FOUND)
  message(STATUS "benchmark found")
  if(JPEGXL_STATIC AND NOT MINGW)
    # benchmark::benchmark hardcodes the librt.so which obviously doesn't
    # compile in static mode.
    set_target_properties(benchmark::benchmark PROPERTIES
      INTERFACE_LINK_LIBRARIES "Threads::Threads;-lrt")
  endif()

  list(APPEND JPEGXL_INTERNAL_TESTS
    # TODO(eustas): Move this to tools/
    ../tools/gauss_blur_gbench.cc
  )

  # Compiles all the benchmark files into a single binary. Individual benchmarks
  # can be run with --benchmark_filter.
  add_executable(jxl_gbench "${JPEGXL_INTERNAL_GBENCH_SOURCES}" gbench_main.cc)

  target_compile_definitions(jxl_gbench PRIVATE
    -DTEST_DATA_PATH="${JPEGXL_TEST_DATA_PATH}")
  target_link_libraries(jxl_gbench
    jxl_extras-internal
    jxl-internal
    jxl_tool
    benchmark::benchmark
  )
else()
  message(STATUS "benchmark NOT found")
endif() # benchmark_FOUND
