/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/audio_converter.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"

namespace webrtc {

typedef rtc::scoped_ptr<ChannelBuffer<float>> ScopedBuffer;

// Sets the signal value to increase by |data| with every sample.
ScopedBuffer CreateBuffer(const std::vector<float>& data, int frames) {
  const int num_channels = static_cast<int>(data.size());
  ScopedBuffer sb(new ChannelBuffer<float>(frames, num_channels));
  for (int i = 0; i < num_channels; ++i)
    for (int j = 0; j < frames; ++j)
      sb->channels()[i][j] = data[i] * j;
  return sb;
}

void VerifyParams(const ChannelBuffer<float>& ref,
                  const ChannelBuffer<float>& test) {
  EXPECT_EQ(ref.num_channels(), test.num_channels());
  EXPECT_EQ(ref.num_frames(), test.num_frames());
}

// Computes the best SNR based on the error between |ref_frame| and
// |test_frame|. It searches around |expected_delay| in samples between the
// signals to compensate for the resampling delay.
float ComputeSNR(const ChannelBuffer<float>& ref,
                 const ChannelBuffer<float>& test,
                 int expected_delay) {
  VerifyParams(ref, test);
  float best_snr = 0;
  int best_delay = 0;

  // Search within one sample of the expected delay.
  for (int delay = std::max(expected_delay - 1, 0);
       delay <= std::min(expected_delay + 1, ref.num_frames());
       ++delay) {
    float mse = 0;
    float variance = 0;
    float mean = 0;
    for (int i = 0; i < ref.num_channels(); ++i) {
      for (int j = 0; j < ref.num_frames() - delay; ++j) {
        float error = ref.channels()[i][j] - test.channels()[i][j + delay];
        mse += error * error;
        variance += ref.channels()[i][j] * ref.channels()[i][j];
        mean += ref.channels()[i][j];
      }
    }

    const int length = ref.num_channels() * (ref.num_frames() - delay);
    mse /= length;
    variance /= length;
    mean /= length;
    variance -= mean * mean;
    float snr = 100;  // We assign 100 dB to the zero-error case.
    if (mse > 0)
      snr = 10 * std::log10(variance / mse);
    if (snr > best_snr) {
      best_snr = snr;
      best_delay = delay;
    }
  }
  printf("SNR=%.1f dB at delay=%d\n", best_snr, best_delay);
  return best_snr;
}

// Sets the source to a linearly increasing signal for which we can easily
// generate a reference. Runs the AudioConverter and ensures the output has
// sufficiently high SNR relative to the reference.
void RunAudioConverterTest(int src_channels,
                           int src_sample_rate_hz,
                           int dst_channels,
                           int dst_sample_rate_hz) {
  const float kSrcLeft = 0.0002f;
  const float kSrcRight = 0.0001f;
  const float resampling_factor = (1.f * src_sample_rate_hz) /
      dst_sample_rate_hz;
  const float dst_left = resampling_factor * kSrcLeft;
  const float dst_right = resampling_factor * kSrcRight;
  const float dst_mono = (dst_left + dst_right) / 2;
  const int src_frames = src_sample_rate_hz / 100;
  const int dst_frames = dst_sample_rate_hz / 100;

  std::vector<float> src_data(1, kSrcLeft);
  if (src_channels == 2)
    src_data.push_back(kSrcRight);
  ScopedBuffer src_buffer = CreateBuffer(src_data, src_frames);

  std::vector<float> dst_data(1, 0);
  std::vector<float> ref_data;
  if (dst_channels == 1) {
    if (src_channels == 1)
      ref_data.push_back(dst_left);
    else
      ref_data.push_back(dst_mono);
  } else {
    dst_data.push_back(0);
    ref_data.push_back(dst_left);
    if (src_channels == 1)
      ref_data.push_back(dst_left);
    else
      ref_data.push_back(dst_right);
  }
  ScopedBuffer dst_buffer = CreateBuffer(dst_data, dst_frames);
  ScopedBuffer ref_buffer = CreateBuffer(ref_data, dst_frames);

  // The sinc resampler has a known delay, which we compute here.
  const int delay_frames = src_sample_rate_hz == dst_sample_rate_hz ? 0 :
      PushSincResampler::AlgorithmicDelaySeconds(src_sample_rate_hz) *
          dst_sample_rate_hz;
  printf("(%d, %d Hz) -> (%d, %d Hz) ",  // SNR reported on the same line later.
      src_channels, src_sample_rate_hz, dst_channels, dst_sample_rate_hz);

  rtc::scoped_ptr<AudioConverter> converter = AudioConverter::Create(
      src_channels, src_frames, dst_channels, dst_frames);
  converter->Convert(src_buffer->channels(), src_buffer->size(),
                     dst_buffer->channels(), dst_buffer->size());

  EXPECT_LT(43.f,
            ComputeSNR(*ref_buffer.get(), *dst_buffer.get(), delay_frames));
}

TEST(AudioConverterTest, ConversionsPassSNRThreshold) {
  const int kSampleRates[] = {8000, 16000, 32000, 44100, 48000};
  const int kSampleRatesSize = sizeof(kSampleRates) / sizeof(*kSampleRates);
  const int kChannels[] = {1, 2};
  const int kChannelsSize = sizeof(kChannels) / sizeof(*kChannels);
  for (int src_rate = 0; src_rate < kSampleRatesSize; ++src_rate) {
    for (int dst_rate = 0; dst_rate < kSampleRatesSize; ++dst_rate) {
      for (int src_channel = 0; src_channel < kChannelsSize; ++src_channel) {
        for (int dst_channel = 0; dst_channel < kChannelsSize; ++dst_channel) {
          RunAudioConverterTest(kChannels[src_channel], kSampleRates[src_rate],
                                kChannels[dst_channel], kSampleRates[dst_rate]);
        }
      }
    }
  }
}

}  // namespace webrtc
