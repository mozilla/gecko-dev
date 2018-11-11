# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'vie_auto_test',
      'type': 'executable',
      'dependencies': [
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
        '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
        '<(webrtc_root)/modules/modules.gyp:video_render_module_internal_impl',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
        '<(webrtc_root)/test/metrics.gyp:metrics',
        '<(webrtc_root)/test/test.gyp:channel_transport',
        '<(webrtc_root)/test/test.gyp:test_support',
        '<(webrtc_root)/test/test.gyp:field_trial',
        'video_engine_core',
        'libvietest',
      ],
      'sources': [
        'interface/vie_autotest.h',
        'interface/vie_autotest_defines.h',
        'interface/vie_autotest_linux.h',
        'interface/vie_autotest_mac_cocoa.h',
        'interface/vie_autotest_main.h',
        'interface/vie_autotest_window_manager_interface.h',
        'interface/vie_autotest_windows.h',
        'interface/vie_file_based_comparison_tests.h',
        'interface/vie_window_manager_factory.h',
        'interface/vie_window_creator.h',

        # New, fully automated tests
        'automated/legacy_fixture.cc',
        'automated/two_windows_fixture.cc',
        'automated/vie_api_integration_test.cc',
        'automated/vie_extended_integration_test.cc',
        'automated/vie_network_test.cc',
        'automated/vie_standard_integration_test.cc',
        'automated/vie_video_verification_test.cc',

        # Test primitives
        'primitives/base_primitives.cc',
        'primitives/base_primitives.h',
        'primitives/choice_helpers.cc',
        'primitives/choice_helpers.h',
        'primitives/choice_helpers_unittest.cc',
        'primitives/fake_stdin.h',
        'primitives/fake_stdin.cc',
        'primitives/framedrop_primitives.h',
        'primitives/framedrop_primitives.cc',
        'primitives/framedrop_primitives_unittest.cc',
        'primitives/general_primitives.cc',
        'primitives/general_primitives.h',
        'primitives/input_helpers.cc',
        'primitives/input_helpers.h',
        'primitives/input_helpers_unittest.cc',

        # Platform independent
        'source/vie_autotest.cc',
        'source/vie_autotest_base.cc',
        'source/vie_autotest_capture.cc',
        'source/vie_autotest_codec.cc',
        'source/vie_autotest_image_process.cc',
        'source/vie_autotest_loopback.cc',
        'source/vie_autotest_main.cc',
        'source/vie_autotest_render.cc',
        'source/vie_autotest_record.cc',
        'source/vie_autotest_rtp_rtcp.cc',
        'source/vie_autotest_custom_call.cc',
        'source/vie_autotest_simulcast.cc',
        'source/vie_file_based_comparison_tests.cc',
        'source/vie_window_creator.cc',

        # Platform dependent
        # Android
        'source/vie_autotest_android.cc',
        # Linux
        'source/vie_autotest_linux.cc',
        'source/vie_window_manager_factory_linux.cc',
        # Mac
        'source/vie_autotest_cocoa_mac.mm',
        'source/vie_window_manager_factory_mac.mm',
        # Windows
        'source/vie_autotest_win.cc',
        'source/vie_window_manager_factory_win.cc',
      ],
      'conditions': [
        ['OS=="android"', {
          'libraries': [
            '-lGLESv2',
            '-llog',
          ],
        }],
        ['OS=="linux"', {
          # TODO(andrew): These should be provided directly by the projects
          #               which require them instead.
          'libraries': [
            '-lXext',
            '-lX11',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            # Use a special main for mac so we can access the webcam.
            '<(webrtc_root)/test/test.gyp:test_support_main_threaded_mac',
          ],
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
            ],
          },
        }],
      ], # conditions
      # Disable warnings to enable Win64 build, issue 1323.
      'msvs_disabled_warnings': [
        4267,  # size_t to int truncation.
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'vie_auto_test_run',
          'type': 'none',
          'dependencies': [
            'vie_auto_test',
          ],
          'includes': [
            '../../../build/isolate.gypi',
          ],
          'sources': [
            'vie_auto_test.isolate',
          ],
        },
      ],
    }],
  ],
}
