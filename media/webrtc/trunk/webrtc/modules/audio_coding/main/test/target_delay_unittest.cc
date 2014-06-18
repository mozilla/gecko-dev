/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/common.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/pcm16b.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace webrtc {


class TargetDelayTest {
 public:
  explicit TargetDelayTest(const Config& config)
      : acm_(config.Get<AudioCodingModuleFactory>().Create(0)) {}

  ~TargetDelayTest() {}

  void SetUp() {
    EXPECT_TRUE(acm_.get() != NULL);

    CodecInst codec;
    ASSERT_EQ(0, AudioCodingModule::Codec("L16", &codec, kSampleRateHz, 1));
    ASSERT_EQ(0, acm_->InitializeReceiver());
    ASSERT_EQ(0, acm_->RegisterReceiveCodec(codec));

    rtp_info_.header.payloadType = codec.pltype;
    rtp_info_.header.timestamp = 0;
    rtp_info_.header.ssrc = 0x12345678;
    rtp_info_.header.markerBit = false;
    rtp_info_.header.sequenceNumber = 0;
    rtp_info_.type.Audio.channel = 1;
    rtp_info_.type.Audio.isCNG = false;
    rtp_info_.frameType = kAudioFrameSpeech;

    int16_t audio[kFrameSizeSamples];
    const int kRange = 0x7FF;  // 2047, easy for masking.
    for (int n = 0; n < kFrameSizeSamples; ++n)
      audio[n] = (rand() & kRange) - kRange / 2;
    WebRtcPcm16b_Encode(audio, kFrameSizeSamples, payload_);
  }

  void OutOfRangeInput() {
    EXPECT_EQ(-1, SetMinimumDelay(-1));
    EXPECT_EQ(-1, SetMinimumDelay(10001));
  }

  void NoTargetDelayBufferSizeChanges() {
    for (int n = 0; n < 30; ++n)  // Run enough iterations.
      Run(true);
    int clean_optimal_delay = GetCurrentOptimalDelayMs();
    Run(false);  // Run with jitter.
    int jittery_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_GT(jittery_optimal_delay, clean_optimal_delay);
    int required_delay = RequiredDelay();
    EXPECT_GT(required_delay, 0);
    EXPECT_NEAR(required_delay, jittery_optimal_delay, 1);
  }

  void WithTargetDelayBufferNotChanging() {
    // A target delay that is one packet larger than jitter.
    const int kTargetDelayMs = (kInterarrivalJitterPacket + 1) *
        kNum10msPerFrame * 10;
    ASSERT_EQ(0, SetMinimumDelay(kTargetDelayMs));
    for (int n = 0; n < 30; ++n)  // Run enough iterations to fill the buffer.
      Run(true);
    int clean_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetDelayMs, clean_optimal_delay);
    Run(false);  // Run with jitter.
    int jittery_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(jittery_optimal_delay, clean_optimal_delay);
  }

  void RequiredDelayAtCorrectRange() {
    for (int n = 0; n < 30; ++n)  // Run clean and store delay.
      Run(true);
    int clean_optimal_delay = GetCurrentOptimalDelayMs();

    // A relatively large delay.
    const int kTargetDelayMs = (kInterarrivalJitterPacket + 10) *
        kNum10msPerFrame * 10;
    ASSERT_EQ(0, SetMinimumDelay(kTargetDelayMs));
    for (int n = 0; n < 300; ++n)  // Run enough iterations to fill the buffer.
      Run(true);
    Run(false);  // Run with jitter.

    int jittery_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetDelayMs, jittery_optimal_delay);

    int required_delay = RequiredDelay();

    // Checking |required_delay| is in correct range.
    EXPECT_GT(required_delay, 0);
    EXPECT_GT(jittery_optimal_delay, required_delay);
    EXPECT_GT(required_delay, clean_optimal_delay);

    // A tighter check for the value of |required_delay|.
    // The jitter forces a delay of
    // |kInterarrivalJitterPacket * kNum10msPerFrame * 10| milliseconds. So we
    // expect |required_delay| be close to that.
    EXPECT_NEAR(kInterarrivalJitterPacket * kNum10msPerFrame * 10,
                required_delay, 1);
  }

  void TargetDelayBufferMinMax() {
    const int kTargetMinDelayMs = kNum10msPerFrame * 10;
    ASSERT_EQ(0, SetMinimumDelay(kTargetMinDelayMs));
    for (int m = 0; m < 30; ++m)  // Run enough iterations to fill the buffer.
      Run(true);
    int clean_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetMinDelayMs, clean_optimal_delay);

    const int kTargetMaxDelayMs = 2 * (kNum10msPerFrame * 10);
    ASSERT_EQ(0, SetMaximumDelay(kTargetMaxDelayMs));
    for (int n = 0; n < 30; ++n)  // Run enough iterations to fill the buffer.
      Run(false);

    int capped_optimal_delay = GetCurrentOptimalDelayMs();
    EXPECT_EQ(kTargetMaxDelayMs, capped_optimal_delay);
  }

 private:
  static const int kSampleRateHz = 16000;
  static const int kNum10msPerFrame = 2;
  static const int kFrameSizeSamples = 320;  // 20 ms @ 16 kHz.
  // payload-len = frame-samples * 2 bytes/sample.
  static const int kPayloadLenBytes = 320 * 2;
  // Inter-arrival time in number of packets in a jittery channel. One is no
  // jitter.
  static const int kInterarrivalJitterPacket = 2;

  void Push() {
    rtp_info_.header.timestamp += kFrameSizeSamples;
    rtp_info_.header.sequenceNumber++;
    ASSERT_EQ(0, acm_->IncomingPacket(payload_, kFrameSizeSamples * 2,
                                      rtp_info_));
  }

  // Pull audio equivalent to the amount of audio in one RTP packet.
  void Pull() {
    AudioFrame frame;
    for (int k = 0; k < kNum10msPerFrame; ++k) {  // Pull one frame.
      ASSERT_EQ(0, acm_->PlayoutData10Ms(-1, &frame));
      // Had to use ASSERT_TRUE, ASSERT_EQ generated error.
      ASSERT_TRUE(kSampleRateHz == frame.sample_rate_hz_);
      ASSERT_EQ(1, frame.num_channels_);
      ASSERT_TRUE(kSampleRateHz / 100 == frame.samples_per_channel_);
    }
  }

  void Run(bool clean) {
    for (int n = 0; n < 10; ++n) {
      for (int m = 0; m < 5; ++m) {
        Push();
        Pull();
      }

      if (!clean) {
        for (int m = 0; m < 10; ++m) {  // Long enough to trigger delay change.
          Push();
          for (int n = 0; n < kInterarrivalJitterPacket; ++n)
            Pull();
        }
      }
    }
  }

  int SetMinimumDelay(int delay_ms) {
    return acm_->SetMinimumPlayoutDelay(delay_ms);
  }

  int SetMaximumDelay(int delay_ms) {
    return acm_->SetMaximumPlayoutDelay(delay_ms);
  }

  int GetCurrentOptimalDelayMs() {
    ACMNetworkStatistics stats;
    acm_->NetworkStatistics(&stats);
    return stats.preferredBufferSize;
  }

  int RequiredDelay() {
    return acm_->LeastRequiredDelayMs();
  }

  scoped_ptr<AudioCodingModule> acm_;
  WebRtcRTPHeader rtp_info_;
  uint8_t payload_[kPayloadLenBytes];
};


namespace {

TargetDelayTest* CreateLegacy() {
  Config config;
  UseLegacyAcm(&config);
  TargetDelayTest* test = new TargetDelayTest(config);
  test->SetUp();
  return test;
}

TargetDelayTest* CreateNew() {
  Config config;
  UseNewAcm(&config);
  TargetDelayTest* test = new TargetDelayTest(config);
  test->SetUp();
  return test;
}

}  // namespace

TEST(TargetDelayTest, DISABLED_ON_ANDROID(OutOfRangeInput)) {
  scoped_ptr<TargetDelayTest> test(CreateLegacy());
  test->OutOfRangeInput();

  test.reset(CreateNew());
  test->OutOfRangeInput();
}

TEST(TargetDelayTest, DISABLED_ON_ANDROID(NoTargetDelayBufferSizeChanges)) {
  scoped_ptr<TargetDelayTest> test(CreateLegacy());
  test->NoTargetDelayBufferSizeChanges();

  test.reset(CreateNew());
  test->NoTargetDelayBufferSizeChanges();
}

TEST(TargetDelayTest, DISABLED_ON_ANDROID(WithTargetDelayBufferNotChanging)) {
  scoped_ptr<TargetDelayTest> test(CreateLegacy());
  test->WithTargetDelayBufferNotChanging();

  test.reset(CreateNew());
  test->WithTargetDelayBufferNotChanging();
}

TEST(TargetDelayTest, DISABLED_ON_ANDROID(RequiredDelayAtCorrectRange)) {
  scoped_ptr<TargetDelayTest> test(CreateLegacy());
  test->RequiredDelayAtCorrectRange();

  test.reset(CreateNew());
  test->RequiredDelayAtCorrectRange();
}

TEST(TargetDelayTest, DISABLED_ON_ANDROID(TargetDelayBufferMinMax)) {
  scoped_ptr<TargetDelayTest> test(CreateLegacy());
  test->TargetDelayBufferMinMax();

  test.reset(CreateNew());
  test->TargetDelayBufferMinMax();
}

}  // namespace webrtc

