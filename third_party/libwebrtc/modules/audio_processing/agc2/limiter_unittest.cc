/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/limiter.h"

#include "api/audio/audio_frame.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/agc2_testing_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {

TEST(Limiter, LimiterShouldConstructAndRun) {
  const size_t samples_per_channel = SampleRateToDefaultChannelSize(48000);
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(&apm_data_dumper, samples_per_channel, "");

  VectorFloatFrame vectors_with_float_frame(1, samples_per_channel,
                                            kMaxAbsFloatS16Value);
  limiter.Process(vectors_with_float_frame.float_frame_view());
}

TEST(Limiter, OutputVolumeAboveThreshold) {
  const size_t samples_per_channel = SampleRateToDefaultChannelSize(48000);
  const float input_level =
      (kMaxAbsFloatS16Value + DbfsToFloatS16(test::kLimiterMaxInputLevelDbFs)) /
      2.f;
  ApmDataDumper apm_data_dumper(0);

  Limiter limiter(&apm_data_dumper, samples_per_channel, "");

  // Give the level estimator time to adapt.
  for (int i = 0; i < 5; ++i) {
    VectorFloatFrame vectors_with_float_frame(1, samples_per_channel,
                                              input_level);
    limiter.Process(vectors_with_float_frame.float_frame_view());
  }

  VectorFloatFrame vectors_with_float_frame(1, samples_per_channel,
                                            input_level);
  limiter.Process(vectors_with_float_frame.float_frame_view());
  rtc::ArrayView<const float> channel =
      vectors_with_float_frame.float_frame_view().channel(0);

  for (const auto& sample : channel) {
    EXPECT_LT(0.9f * kMaxAbsFloatS16Value, sample);
  }
}

}  // namespace webrtc
