# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file exists in two versions. A no-op version under
# webrtc/build/apk_tests_noop.gyp and this one. This gyp file builds the apk
# unit tests (for Android) assuming that WebRTC is built inside a Chromium
# workspace. The no-op version is included when building WebRTC without
# Chromium. This is a workaround for the fact that 'includes' don't expand
# variables and that the relative location of apk_test.gypi is different for
# WebRTC when built as part of Chromium and when it is built without Chromium.
{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'audio_decoder_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'audio_decoder_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)audio_decoder_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:audio_decoder_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'common_audio_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'common_audio_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)common_audio_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'common_video_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'common_video_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)common_video_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/common_video/common_video_unittests.gyp:common_video_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'modules_tests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'modules_tests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)modules_tests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:modules_tests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'modules_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'modules_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)modules_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:modules_unittests',
        'audio_device_java',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'rtc_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'rtc_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)rtc_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/webrtc.gyp:rtc_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'system_wrappers_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'system_wrappers_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)system_wrappers_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/system_wrappers/system_wrappers_tests.gyp:system_wrappers_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'test_support_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'test_support_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)test_support_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/test/test.gyp:test_support_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'tools_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'tools_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)tools_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/tools/tools.gyp:tools_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'video_engine_core_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'video_engine_core_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)video_engine_core_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/video_engine/video_engine.gyp:video_engine_core_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'video_engine_tests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'video_engine_tests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)video_engine_tests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/webrtc.gyp:video_engine_tests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
     },
     {
      'target_name': 'voice_engine_unittests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'voice_engine_unittests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)voice_engine_unittests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine_unittests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'webrtc_perf_tests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'webrtc_perf_tests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)webrtc_perf_tests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/webrtc.gyp:webrtc_perf_tests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'audio_codec_speed_tests_apk',
      'type': 'none',
      'variables': {
        'test_suite_name': 'audio_codec_speed_tests',
        'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)audio_codec_speed_tests<(SHARED_LIB_SUFFIX)',
      },
      'dependencies': [
        '<(webrtc_root)/modules/modules.gyp:audio_codec_speed_tests',
      ],
      'includes': [
        '../../build/apk_test.gypi',
      ],
    },
    {
      'target_name': 'video_capture_tests_apk',
      'type': 'none',
       'variables': {
         'test_suite_name': 'video_capture_tests',
         'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)video_capture_tests<(SHARED_LIB_SUFFIX)',
       },
       'dependencies': [
         '<(webrtc_root)/modules/modules.gyp:video_capture_tests',
         'video_capture_java',
       ],
       'includes': [
         '../../build/apk_test.gypi',
       ],
    },
    {
      # Used only by video_capture_tests_apk above, and impossible to use in the
      # standalone build, which is why it's declared here instead of under
      # modules/video_capture/ (to avoid the need for a forked _noop.gyp file
      # like this file has; see comment at the top of this file).
      'target_name': 'video_capture_java',
      'type': 'none',
      'variables': {
        'java_in_dir': '<(webrtc_root)/modules/video_capture/android/java',
      },
      'includes': [
        '../../build/java.gypi',
      ],
    },
    {
      'target_name': 'audio_device_java',
      'type': 'none',
      'variables': {
        'java_in_dir': '<(webrtc_root)/modules/audio_device/android/java',
        'never_lint': 1,
      },
      'includes': [
        '../../build/java.gypi',
      ],
    },
  ],
}


