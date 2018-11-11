# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi',],
  'targets': [
    {
      'target_name': 'system_wrappers_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/test/test.gyp:test_support_main',
      ],
      'sources': [
        'source/aligned_array_unittest.cc',
        'source/aligned_malloc_unittest.cc',
        'source/clock_unittest.cc',
        'source/condition_variable_unittest.cc',
        'source/critical_section_unittest.cc',
        'source/event_tracer_unittest.cc',
        'source/logging_unittest.cc',
        'source/data_log_unittest.cc',
        'source/data_log_unittest_disabled.cc',
        'source/data_log_helpers_unittest.cc',
        'source/data_log_c_helpers_unittest.c',
        'source/data_log_c_helpers_unittest.h',
        'source/rtp_to_ntp_unittest.cc',
        'source/scoped_vector_unittest.cc',
        'source/stringize_macros_unittest.cc',
        'source/stl_util_unittest.cc',
        'source/thread_unittest.cc',
        'source/thread_posix_unittest.cc',
      ],
      'conditions': [
        ['enable_data_logging==1', {
          'sources!': [ 'source/data_log_unittest_disabled.cc', ],
        }, {
          'sources!': [ 'source/data_log_unittest.cc', ],
        }],
        ['os_posix==0', {
          'sources!': [ 'source/thread_posix_unittest.cc', ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ],
  'conditions': [
    ['include_tests==1 and OS=="android"', {
      'targets': [
        {
          'target_name': 'system_wrappers_unittests_apk_target',
          'type': 'none',
          'dependencies': [
            '<(apk_tests_path):system_wrappers_unittests_apk',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'system_wrappers_unittests_run',
          'type': 'none',
          'dependencies': [
            'system_wrappers_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'system_wrappers_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}

