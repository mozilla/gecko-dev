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
PushResampler<T>::PushResampler() = default;

template <typename T>
PushResampler<T>::~PushResampler() = default;

template <typename T>
int PushResampler<T>::InitializeIfNeeded(int src_sample_rate_hz,
                                         int dst_sample_rate_hz,
                                         size_t num_channels) {
  // These checks used to be factored out of this template function due to
  // Windows debug build issues with clang. http://crbug.com/615050
  RTC_CHECK_GT(src_sample_rate_hz, 0);
  RTC_CHECK_GT(dst_sample_rate_hz, 0);
  RTC_CHECK_GT(num_channels, 0);

  const size_t src_size_10ms_mono =
      static_cast<size_t>(src_sample_rate_hz / 100);
  const size_t dst_size_10ms_mono =
      static_cast<size_t>(dst_sample_rate_hz / 100);

  if (src_size_10ms_mono == SamplesPerChannel(source_view_) &&
      dst_size_10ms_mono == SamplesPerChannel(destination_view_) &&
      num_channels == NumChannels(source_view_)) {
    // No-op if settings haven't changed.
    return 0;
  }

  // Allocate two buffers for all source and destination channels.
  // Then organize source and destination views together with an array of
  // resamplers for each channel in the deinterlaved buffers.
  source_.reset(new T[src_size_10ms_mono * num_channels]);
  destination_.reset(new T[dst_size_10ms_mono * num_channels]);
  source_view_ =
      DeinterleavedView<T>(source_.get(), src_size_10ms_mono, num_channels);
  destination_view_ = DeinterleavedView<T>(destination_.get(),
                                           dst_size_10ms_mono, num_channels);
  resamplers_.resize(num_channels);
  for (size_t i = 0; i < num_channels; ++i) {
    resamplers_[i] = std::make_unique<PushSincResampler>(src_size_10ms_mono,
                                                         dst_size_10ms_mono);
  }

  return 0;
}

template <typename T>
int PushResampler<T>::Resample(InterleavedView<const T> src,
                               InterleavedView<T> dst) {
  RTC_DCHECK_EQ(NumChannels(src), NumChannels(source_view_));
  RTC_DCHECK_EQ(NumChannels(dst), NumChannels(destination_view_));
  RTC_DCHECK_EQ(SamplesPerChannel(src), SamplesPerChannel(source_view_));
  RTC_DCHECK_EQ(SamplesPerChannel(dst), SamplesPerChannel(destination_view_));

  if (SamplesPerChannel(src) == SamplesPerChannel(dst)) {
    // The old resampler provides this memcpy facility in the case of matching
    // sample rates, so reproduce it here for the sinc resampler.
    CopySamples(dst, src);
    return static_cast<int>(src.data().size());
  }

  Deinterleave(src, source_view_);

  for (size_t i = 0; i < resamplers_.size(); ++i) {
    size_t dst_length_mono =
        resamplers_[i]->Resample(source_view_[i], destination_view_[i]);
    RTC_DCHECK_EQ(dst_length_mono, SamplesPerChannel(dst));
  }

  Interleave<T>(destination_view_, dst);
  return static_cast<int>(dst.size());
}

template <typename T>
int PushResampler<T>::Resample(MonoView<const T> src, MonoView<T> dst) {
  RTC_DCHECK_EQ(resamplers_.size(), 1);
  RTC_DCHECK_EQ(SamplesPerChannel(src), SamplesPerChannel(source_view_));
  RTC_DCHECK_EQ(SamplesPerChannel(dst), SamplesPerChannel(destination_view_));

  if (SamplesPerChannel(src) == SamplesPerChannel(dst)) {
    CopySamples(dst, src);
    return static_cast<int>(src.size());
  }

  return resamplers_[0]->Resample(src, dst);
}

// Explictly generate required instantiations.
template class PushResampler<int16_t>;
template class PushResampler<float>;

}  // namespace webrtc
