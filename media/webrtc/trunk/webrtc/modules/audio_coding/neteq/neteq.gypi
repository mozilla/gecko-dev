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
      'target_name': 'NetEq',
      'type': 'static_library',
      'dependencies': [
        'CNG',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
      ],
      'defines': [
        'NETEQ_VOICEENGINE_CODECS', # TODO: Should create a Chrome define which
        'SCRATCH',                  # specifies a subset of codecs to support.
      ],
      'include_dirs': [
        'interface',
        '<(webrtc_root)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
          '<(webrtc_root)',
        ],
      },
      'sources': [
        'interface/webrtc_neteq.h',
        'interface/webrtc_neteq_help_macros.h',
        'interface/webrtc_neteq_internal.h',
        'accelerate.c',
        'automode.c',
        'automode.h',
        'bgn_update.c',
        'buffer_stats.h',
        'bufstats_decision.c',
        'cng_internal.c',
        'codec_db.c',
        'codec_db.h',
        'codec_db_defines.h',
        'correlator.c',
        'delay_logging.h',
        'dsp.c',
        'dsp.h',
        'dsp_helpfunctions.c',
        'dsp_helpfunctions.h',
        'dtmf_buffer.c',
        'dtmf_buffer.h',
        'dtmf_tonegen.c',
        'dtmf_tonegen.h',
        'expand.c',
        'mcu.h',
        'mcu_address_init.c',
        'mcu_dsp_common.c',
        'mcu_dsp_common.h',
        'mcu_reset.c',
        'merge.c',
        'min_distortion.c',
        'mix_voice_unvoice.c',
        'mute_signal.c',
        'neteq_defines.h',
        'neteq_error_codes.h',
        'neteq_statistics.h',
        'normal.c',
        'packet_buffer.c',
        'packet_buffer.h',
        'peak_detection.c',
        'preemptive_expand.c',
        'random_vector.c',
        'recin.c',
        'recout.c',
        'rtcp.c',
        'rtcp.h',
        'rtp.c',
        'rtp.h',
        'set_fs.c',
        'signal_mcu.c',
        'split_and_insert.c',
        'unmute_signal.c',
        'webrtc_neteq.c',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'neteq_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'NetEq',
            'NetEqTestTools',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'webrtc_neteq_unittest.cc',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
          'conditions': [
            # TODO(henrike): remove build_with_chromium==1 when the bots are
            # using Chromium's buildbots.
            ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }, # neteq_unittests
        {
          'target_name': 'NetEqRTPplay',
          'type': 'executable',
          'dependencies': [
            'NetEq',          # NetEQ library defined above
            'NetEqTestTools', # Test helpers
            'G711',
            'G722',
            'PCM16B',
            'iLBC',
            'iSAC',
            'CNG',
          ],
          'defines': [
            # TODO: Make codec selection conditional on definitions in target NetEq
            'CODEC_ILBC',
            'CODEC_PCM16B',
            'CODEC_G711',
            'CODEC_G722',
            'CODEC_ISAC',
            'CODEC_PCM16B_WB',
            'CODEC_ISAC_SWB',
            'CODEC_ISAC_FB',
            'CODEC_PCM16B_32KHZ',
            'CODEC_CNGCODEC8',
            'CODEC_CNGCODEC16',
            'CODEC_CNGCODEC32',
            'CODEC_ATEVENT_DECODE',
            'CODEC_RED',
          ],
          'include_dirs': [
            '.',
            'test',
          ],
          'sources': [
            'test/NetEqRTPplay.cc',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
        },

        {
          'target_name': 'neteq3_speed_test',
          'type': 'executable',
          'dependencies': [
            'NetEq',
            'PCM16B',
            'neteq_unittest_tools',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
          'sources': [
            'test/neteq_speed_test.cc',
          ],
        },

        {
         'target_name': 'NetEqTestTools',
          # Collection of useful functions used in other tests
          'type': 'static_library',
          'variables': {
            # Expects RTP packets without payloads when enabled.
            'neteq_dummy_rtp%': 0,
          },
          'dependencies': [
            'G711',
            'G722',
            'PCM16B',
            'iLBC',
            'iSAC',
            'CNG',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'interface',
              'test',
            ],
          },
          'defines': [
            # TODO: Make codec selection conditional on definitions in target NetEq
            'CODEC_ILBC',
            'CODEC_PCM16B',
            'CODEC_G711',
            'CODEC_G722',
            'CODEC_ISAC',
            'CODEC_PCM16B_WB',
            'CODEC_ISAC_SWB',
            'CODEC_ISAC_FB',
            'CODEC_PCM16B_32KHZ',
            'CODEC_CNGCODEC8',
            'CODEC_CNGCODEC16',
            'CODEC_CNGCODEC32',
            'CODEC_ATEVENT_DECODE',
            'CODEC_RED',
          ],
          'include_dirs': [
            'interface',
            'test',
          ],
          'sources': [
            'test/NETEQTEST_CodecClass.cc',
            'test/NETEQTEST_CodecClass.h',
            'test/NETEQTEST_DummyRTPpacket.cc',
            'test/NETEQTEST_DummyRTPpacket.h',
            'test/NETEQTEST_NetEQClass.cc',
            'test/NETEQTEST_NetEQClass.h',
            'test/NETEQTEST_RTPpacket.cc',
            'test/NETEQTEST_RTPpacket.h',
          ],
          # Disable warnings to enable Win64 build, issue 1323.
          'msvs_disabled_warnings': [
            4267,  # size_t to int truncation.
          ],
        },
      ], # targets
      'conditions': [
        # TODO(henrike): remove build_with_chromium==1 when the bots are using
        # Chromium's buildbots.
        ['build_with_chromium==1 and OS=="android" and gtest_target_type=="shared_library"', {
          'targets': [
            {
              'target_name': 'neteq_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):neteq_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'neteq_unittests_run',
              'type': 'none',
              'dependencies': [
                'neteq_unittests',
              ],
              'includes': [
                '../../../build/isolate.gypi',
                'neteq_unittests.isolate',
              ],
              'sources': [
                'neteq_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ], # conditions
}
