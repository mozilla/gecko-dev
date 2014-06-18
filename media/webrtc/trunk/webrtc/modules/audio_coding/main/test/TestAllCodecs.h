/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTALLCODECS_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTALLCODECS_H_

#include "webrtc/common.h"
#include "webrtc/modules/audio_coding/main/test/ACMTest.h"
#include "webrtc/modules/audio_coding/main/test/Channel.h"
#include "webrtc/modules/audio_coding/main/test/PCMFile.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Config;

class TestPack : public AudioPacketizationCallback {
 public:
  TestPack();
  ~TestPack();

  void RegisterReceiverACM(AudioCodingModule* acm);

  int32_t SendData(FrameType frame_type, uint8_t payload_type,
                   uint32_t timestamp, const uint8_t* payload_data,
                   uint16_t payload_size,
                   const RTPFragmentationHeader* fragmentation);

  uint16_t payload_size();
  uint32_t timestamp_diff();
  void reset_payload_size();

 private:
  AudioCodingModule* receiver_acm_;
  uint16_t sequence_number_;
  uint8_t payload_data_[60 * 32 * 2 * 2];
  uint32_t timestamp_diff_;
  uint32_t last_in_timestamp_;
  uint64_t total_bytes_;
  uint16_t payload_size_;
};

class TestAllCodecs : public ACMTest {
 public:
  TestAllCodecs(int test_mode, const Config& config);
  ~TestAllCodecs();

  void Perform();

 private:
  // The default value of '-1' indicates that the registration is based only on
  // codec name, and a sampling frequency matching is not required.
  // This is useful for codecs which support several sampling frequency.
  // Note! Only mono mode is tested in this test.
  void RegisterSendCodec(char side, char* codec_name, int32_t sampling_freq_hz,
                         int rate, int packet_size, int extra_byte);

  void Run(TestPack* channel);
  void OpenOutFile(int test_number);
  void DisplaySendReceiveCodec();

  int test_mode_;
  scoped_ptr<AudioCodingModule> acm_a_;
  scoped_ptr<AudioCodingModule> acm_b_;
  TestPack* channel_a_to_b_;
  PCMFile infile_a_;
  PCMFile outfile_b_;
  int test_count_;
  uint16_t packet_size_samples_;
  uint16_t packet_size_bytes_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_TEST_TESTALLCODECS_H_
