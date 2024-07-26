/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/resampler/include/push_resampler.h"

#include <stdint.h>
#include <string.h>

#include <memory>

#include "api/audio/audio_frame.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/resampler/push_sinc_resampler.h"
#include "rtc_base/checks.h"

namespace webrtc {

template <typename T>
PushResampler<T>::PushResampler()
    : src_sample_rate_hz_(0), dst_sample_rate_hz_(0), num_channels_(0) {}

template <typename T>
PushResampler<T>::~PushResampler() {}

template <typename T>
int PushResampler<T>::InitializeIfNeeded(int src_sample_rate_hz,
                                         int dst_sample_rate_hz,
                                         size_t num_channels) {
  // These checks used to be factored out of this template function due to
  // Windows debug build issues with clang. http://crbug.com/615050
  RTC_DCHECK_GT(src_sample_rate_hz, 0);
  RTC_DCHECK_GT(dst_sample_rate_hz, 0);
  RTC_DCHECK_GT(num_channels, 0);

  if (src_sample_rate_hz == src_sample_rate_hz_ &&
      dst_sample_rate_hz == dst_sample_rate_hz_ &&
      num_channels == num_channels_) {
    // No-op if settings haven't changed.
    return 0;
  }

  if (src_sample_rate_hz <= 0 || dst_sample_rate_hz <= 0 || num_channels <= 0) {
    return -1;
  }

  src_sample_rate_hz_ = src_sample_rate_hz;
  dst_sample_rate_hz_ = dst_sample_rate_hz;
  num_channels_ = num_channels;

  // TODO: b/335805780 - Change this to use a single buffer for source and
  // destination and initialize each ChannelResampler() with a pointer to
  // channels in each deinterleaved buffer. That way, DeinterleavedView can be
  // used for the two buffers.

  const size_t src_size_10ms_mono =
      static_cast<size_t>(src_sample_rate_hz / 100);
  const size_t dst_size_10ms_mono =
      static_cast<size_t>(dst_sample_rate_hz / 100);
  channel_resamplers_.clear();
  for (size_t i = 0; i < num_channels; ++i) {
    channel_resamplers_.push_back(ChannelResampler());
    auto channel_resampler = channel_resamplers_.rbegin();
    channel_resampler->resampler = std::make_unique<PushSincResampler>(
        src_size_10ms_mono, dst_size_10ms_mono);
    channel_resampler->source.resize(src_size_10ms_mono);
    channel_resampler->destination.resize(dst_size_10ms_mono);
  }

  channel_data_array_.resize(num_channels_);

  return 0;
}

template <typename T>
int PushResampler<T>::Resample(InterleavedView<const T> src,
                               InterleavedView<T> dst) {
  RTC_DCHECK_EQ(NumChannels(src), num_channels_);
  RTC_DCHECK_EQ(NumChannels(dst), num_channels_);
  RTC_DCHECK_EQ(SamplesPerChannel(src),
                SampleRateToDefaultChannelSize(src_sample_rate_hz_));
  RTC_DCHECK_EQ(SamplesPerChannel(dst),
                SampleRateToDefaultChannelSize(dst_sample_rate_hz_));

  if (src_sample_rate_hz_ == dst_sample_rate_hz_) {
    // The old resampler provides this memcpy facility in the case of matching
    // sample rates, so reproduce it here for the sinc resampler.
    CopySamples(dst, src);
    return static_cast<int>(src.data().size());
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    channel_data_array_[ch] = channel_resamplers_[ch].source.data();
  }

  // TODO: b/335805780 - Deinterleave should accept InterleavedView<> as input.
  Deinterleave(&src.data()[0], src.samples_per_channel(), src.num_channels(),
               channel_data_array_.data());

  for (auto& resampler : channel_resamplers_) {
    size_t dst_length_mono = resampler.resampler->Resample(
        resampler.source.data(), src.samples_per_channel(),
        resampler.destination.data(), dst.samples_per_channel());
    RTC_DCHECK_EQ(dst_length_mono, dst.samples_per_channel());
  }

  for (size_t ch = 0; ch < num_channels_; ++ch) {
    channel_data_array_[ch] = channel_resamplers_[ch].destination.data();
  }

  // TODO: b/335805780 - Interleave should accept DeInterleavedView<> as src.
  Interleave(channel_data_array_.data(), dst.samples_per_channel(),
             num_channels_, dst);
  return static_cast<int>(dst.size());
}

// Explictly generate required instantiations.
template class PushResampler<int16_t>;
template class PushResampler<float>;

}  // namespace webrtc
