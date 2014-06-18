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
      'target_name': 'video_capture_module',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/common_video/common_video.gyp:common_video',
        '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'cflags_mozilla': [
        '$(NSPR_CFLAGS)',
      ],
      'sources': [
        'device_info_impl.cc',
        'device_info_impl.h',
        'include/video_capture.h',
        'include/video_capture_defines.h',
        'include/video_capture_factory.h',
        'video_capture_config.h',
        'video_capture_delay.h',
        'video_capture_factory.cc',
        'video_capture_impl.cc',
        'video_capture_impl.h',
      ],
      'conditions': [
        ['include_internal_video_capture==0', {
          'sources': [
            'external/device_info_external.cc',
            'external/video_capture_external.cc',
          ],
        }, {  # include_internal_video_capture == 1
          'conditions': [
            ['include_v4l2_video_capture==1', {
              'sources': [
                'linux/device_info_linux.cc',
                'linux/device_info_linux.h',
                'linux/video_capture_linux.cc',
                'linux/video_capture_linux.h',
              ],
            }],  # linux
            ['OS=="mac"', {
              'sources': [
                'mac/qtkit/video_capture_qtkit.h',
                'mac/qtkit/video_capture_qtkit.mm',
                'mac/qtkit/video_capture_qtkit_info.h',
                'mac/qtkit/video_capture_qtkit_info.mm',
                'mac/qtkit/video_capture_qtkit_info_objc.h',
                'mac/qtkit/video_capture_qtkit_info_objc.mm',
                'mac/qtkit/video_capture_qtkit_objc.h',
                'mac/qtkit/video_capture_qtkit_objc.mm',
                'mac/qtkit/video_capture_qtkit_utility.h',
                'mac/video_capture_mac.mm',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework QTKit',
                  ],
                },
              },
            }],  # mac
            ['OS=="win"', {
              'conditions': [
                ['build_with_mozilla==0', {
                  'dependencies': [
                    '<(DEPTH)/third_party/winsdk_samples/winsdk_samples.gyp:directshow_baseclasses',
                  ],
                }],
              ],
              'sources': [
                'windows/device_info_ds.cc',
                'windows/device_info_ds.h',
                'windows/device_info_mf.cc',
                'windows/device_info_mf.h',
                'windows/help_functions_ds.cc',
                'windows/help_functions_ds.h',
                'windows/sink_filter_ds.cc',
                'windows/sink_filter_ds.h',
                'windows/video_capture_ds.cc',
                'windows/video_capture_ds.h',
                'windows/video_capture_factory_windows.cc',
                'windows/video_capture_mf.cc',
                'windows/video_capture_mf.h',
                'windows/BasePin.cpp',
                'windows/BaseFilter.cpp',
                'windows/BaseInputPin.cpp',
                'windows/MediaType.cpp',
              ],
              'link_settings': {
                'libraries': [
                  '-lStrmiids.lib',
                ],
              },
            }],  # win
            ['OS=="android"', {
              'sources': [
                'android/device_info_android.cc',
                'android/device_info_android.h',
                'android/video_capture_android.cc',
                'android/video_capture_android.h',
              ],
            }],  # android
            ['OS=="ios"', {
              'sources': [
                'ios/device_info_ios.h',
                'ios/device_info_ios.mm',
                'ios/device_info_ios_objc.h',
                'ios/device_info_ios_objc.mm',
                'ios/video_capture_ios.h',
                'ios/video_capture_ios.mm',
                'ios/video_capture_ios_objc.h',
                'ios/video_capture_ios_objc.mm',
              ],
              'all_dependent_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-framework AVFoundation',
                    '-framework CoreMedia',
                    '-framework CoreVideo',
                    '-framework UIKit',
                  ],
                },
              },
            }],  # ios
          ], # conditions
        }],  # include_internal_video_capture
      ], # conditions
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'video_capture_tests',
          'type': 'executable',
          'dependencies': [
            'video_capture_module',
            'webrtc_utility',
            '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'test/video_capture_unittest.cc',
            'test/video_capture_main_mac.mm',
          ],
          'conditions': [
            ['OS!="win" and OS!="android"', {
              'cflags': [
                '-Wno-write-strings',
              ],
              'ldflags': [
                '-lpthread -lm',
              ],
            }],
            ['include_v4l2_video_capture==1', {
              'libraries': [
                '-lXext',
                '-lX11',
              ],
            }],
            ['OS=="linux"', {
              'libraries': [
                '-lrt',
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
                # Link with a special main for mac so we can use the webcam.
                '<(webrtc_root)/test/test.gyp:test_support_main_threaded_mac',
              ],
              'xcode_settings': {
                # TODO(andrew): CoreAudio and AudioToolbox shouldn't be needed.
                'OTHER_LDFLAGS': [
                  '-framework Foundation -framework AppKit -framework Cocoa -framework OpenGL -framework CoreVideo -framework CoreAudio -framework AudioToolbox',
                ],
              },
            }], # OS=="mac"
            ['OS!="mac"', {
              'dependencies': [
                # Otherwise, use the regular main.
                '<(webrtc_root)/test/test.gyp:test_support_main',
              ],
            }], # OS!="mac"
          ] # conditions
        },
      ], # targets
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'video_capture_tests_run',
              'type': 'none',
              'dependencies': [
                'video_capture_tests',
              ],
              'includes': [
                '../../build/isolate.gypi',
                'video_capture_tests.isolate',
              ],
              'sources': [
                'video_capture_tests.isolate',
              ],
            },
          ],
        }],
      ],
    }],
  ],
}

