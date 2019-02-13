# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'codecs': [
      'G711',
      'PCM16B',
      'CNG',
    ],
    'neteq_defines': [],
    'conditions': [
      ['include_g722==1', {
        'neteq_dependencies': ['G722'],
        'neteq_defines': ['WEBRTC_CODEC_G722',],
      }],
      ['include_ilbc==1', {
        'neteq_dependencies': ['iLBC'],
        'neteq_defines': ['WEBRTC_CODEC_ILBC',],
      }],
      ['include_isac==1', {
        'neteq_dependencies': ['iSAC', 'iSACFix',],
        'neteq_defines': ['WEBRTC_CODEC_ISAC', 'WEBRTC_CODEC_ISACFIX',],
      }],
      ['include_opus==1', {
        'codecs': ['webrtc_opus'],
        'neteq_dependencies': ['webrtc_opus'],
        'neteq_defines': ['WEBRTC_CODEC_OPUS',],
        'conditions': [
          ['build_with_mozilla==0', {
            'neteq_dependencies': [
              '<(DEPTH)/third_party/opus/opus.gyp:opus',
	    ],
	  }],
 	],
      }],
    ],
    'neteq_dependencies': [
      '<@(codecs)',
      '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
      '<(webrtc_root)/system_wrappers/source/system_wrappers.gyp:system_wrappers',
    ],

  },
  'targets': [
    {
      'target_name': 'neteq',
      'type': 'static_library',
      'include_dirs': [
        '../../../../../../media/opus/celt',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../../../../../../media/opus/celt',
	],
      },
      'dependencies': [
        '<@(neteq_dependencies)',
      ],
      'defines': [
        '<@(neteq_defines)',
      ],
      'conditions': [
        ['build_with_mozilla==0', {
          'include_dirs': [
            # Need Opus header files for the audio classifier.
            '<(DEPTH)/third_party/opus/src/celt',
            '<(DEPTH)/third_party/opus/src/src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              # Need Opus header files for the audio classifier.
              '<(DEPTH)/third_party/opus/src/celt',
              '<(DEPTH)/third_party/opus/src/src',
            ],
          },
          'export_dependent_settings': [
            '<(DEPTH)/third_party/opus/opus.gyp:opus',
          ],
	}],
        ['build_with_mozilla==1', {
          'include_dirs': [
            # Need Opus header files for the audio classifier.
            '<(DEPTH)/../../../media/opus/celt',
#            '<(DEPTH)/third_party/opus/src/src',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '../../../../../../media/opus/celt',
              # Need Opus header files for the audio classifier.
              '<(DEPTH)/../../../media/opus/celt',
#              '<(DEPTH)/third_party/opus/src/src',
            ],
          },
        }],
      ],
      'sources': [
        'interface/audio_decoder.h',
        'interface/neteq.h',
        'accelerate.cc',
        'accelerate.h',
        'audio_classifier.cc',
        'audio_classifier.h',
        'audio_decoder_impl.cc',
        'audio_decoder_impl.h',
        'audio_decoder.cc',
        'audio_multi_vector.cc',
        'audio_multi_vector.h',
        'audio_vector.cc',
        'audio_vector.h',
        'background_noise.cc',
        'background_noise.h',
        'buffer_level_filter.cc',
        'buffer_level_filter.h',
        'comfort_noise.cc',
        'comfort_noise.h',
        'decision_logic.cc',
        'decision_logic.h',
        'decision_logic_fax.cc',
        'decision_logic_fax.h',
        'decision_logic_normal.cc',
        'decision_logic_normal.h',
        'decoder_database.cc',
        'decoder_database.h',
        'defines.h',
        'delay_manager.cc',
        'delay_manager.h',
        'delay_peak_detector.cc',
        'delay_peak_detector.h',
        'dsp_helper.cc',
        'dsp_helper.h',
        'dtmf_buffer.cc',
        'dtmf_buffer.h',
        'dtmf_tone_generator.cc',
        'dtmf_tone_generator.h',
        'expand.cc',
        'expand.h',
        'merge.cc',
        'merge.h',
        'neteq_impl.cc',
        'neteq_impl.h',
        'neteq.cc',
        'statistics_calculator.cc',
        'statistics_calculator.h',
        'normal.cc',
        'normal.h',
        'packet_buffer.cc',
        'packet_buffer.h',
        'payload_splitter.cc',
        'payload_splitter.h',
        'post_decode_vad.cc',
        'post_decode_vad.h',
        'preemptive_expand.cc',
        'preemptive_expand.h',
        'random_vector.cc',
        'random_vector.h',
        'rtcp.cc',
        'rtcp.h',
        'sync_buffer.cc',
        'sync_buffer.h',
        'timestamp_scaler.cc',
        'timestamp_scaler.h',
        'time_stretch.cc',
        'time_stretch.h',
      ],
    },
  ], # targets
  'conditions': [
    ['include_tests==1', {
      'includes': ['neteq_tests.gypi',],
      'targets': [
        {
          'target_name': 'audio_decoder_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '<@(codecs)',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
            '<(webrtc_root)/test/test.gyp:test_support_main',
          ],
# FIX for include_isac/etc
          'defines': [
            'AUDIO_DECODER_UNITTEST',
            'WEBRTC_CODEC_G722',
            'WEBRTC_CODEC_ILBC',
            'WEBRTC_CODEC_ISACFX',
            'WEBRTC_CODEC_ISAC',
            'WEBRTC_CODEC_PCM16',
            '<@(neteq_defines)',
          ],
          'sources': [
            'audio_decoder_impl.cc',
            'audio_decoder_impl.h',
            'audio_decoder_unittest.cc',
            'audio_decoder.cc',
            'interface/audio_decoder.h',
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': [
                '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }, # audio_decoder_unittests

        {
          'target_name': 'neteq_unittest_tools',
          'type': 'static_library',
          'dependencies': [
            'rtp_rtcp',
            '<(webrtc_root)/test/test.gyp:rtp_test_utils',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'tools',
            ],
          },
          'include_dirs': [
            'tools',
          ],
          'sources': [
            'tools/audio_checksum.h',
            'tools/audio_loop.cc',
            'tools/audio_loop.h',
            'tools/audio_sink.h',
            'tools/constant_pcm_packet_source.cc',
            'tools/constant_pcm_packet_source.h',
            'tools/input_audio_file.cc',
            'tools/input_audio_file.h',
            'tools/output_audio_file.h',
            'tools/packet.cc',
            'tools/packet.h',
            'tools/packet_source.h',
            'tools/resample_input_audio_file.cc',
            'tools/resample_input_audio_file.h',
            'tools/rtp_file_source.cc',
            'tools/rtp_file_source.h',
            'tools/rtp_generator.cc',
            'tools/rtp_generator.h',
          ],
        }, # neteq_unittest_tools
      ], # targets
      'conditions': [
        ['OS=="android"', {
          'targets': [
            {
              'target_name': 'audio_decoder_unittests_apk_target',
              'type': 'none',
              'dependencies': [
                '<(apk_tests_path):audio_decoder_unittests_apk',
              ],
            },
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'audio_decoder_unittests_run',
              'type': 'none',
              'dependencies': [
                'audio_decoder_unittests',
              ],
              'includes': [
                '../../../build/isolate.gypi',
              ],
              'sources': [
                'audio_decoder_unittests.isolate',
              ],
            },
          ],
        }],
      ],
    }], # include_tests
  ], # conditions
}
