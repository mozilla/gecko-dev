/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/splitting_filter.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class PushSincResampler;
class IFChannelBuffer;

enum Band {
  kBand0To8kHz = 0,
  kBand8To16kHz = 1,
  kBand16To24kHz = 2
};

class AudioBuffer {
 public:
  // TODO(ajm): Switch to take ChannelLayouts.
  AudioBuffer(int input_num_frames,
              int num_input_channels,
              int process_num_frames,
              int num_process_channels,
              int output_num_frames);
  virtual ~AudioBuffer();

  int num_channels() const;
  void set_num_channels(int num_channels);
  int num_frames() const;
  int num_frames_per_band() const;
  int num_keyboard_frames() const;
  int num_bands() const;

  // Returns a pointer array to the full-band channels.
  // Usage:
  // channels()[channel][sample].
  // Where:
  // 0 <= channel < |num_proc_channels_|
  // 0 <= sample < |proc_num_frames_|
  int16_t* const* channels();
  const int16_t* const* channels_const() const;
  float* const* channels_f();
  const float* const* channels_const_f() const;

  // Returns a pointer array to the bands for a specific channel.
  // Usage:
  // split_bands(channel)[band][sample].
  // Where:
  // 0 <= channel < |num_proc_channels_|
  // 0 <= band < |num_bands_|
  // 0 <= sample < |num_split_frames_|
  int16_t* const* split_bands(int channel);
  const int16_t* const* split_bands_const(int channel) const;
  float* const* split_bands_f(int channel);
  const float* const* split_bands_const_f(int channel) const;

  // Returns a pointer array to the channels for a specific band.
  // Usage:
  // split_channels(band)[channel][sample].
  // Where:
  // 0 <= band < |num_bands_|
  // 0 <= channel < |num_proc_channels_|
  // 0 <= sample < |num_split_frames_|
  int16_t* const* split_channels(Band band);
  const int16_t* const* split_channels_const(Band band) const;
  float* const* split_channels_f(Band band);
  const float* const* split_channels_const_f(Band band) const;

  // Returns a pointer to the ChannelBuffer that encapsulates the full-band
  // data.
  ChannelBuffer<int16_t>* data();
  const ChannelBuffer<int16_t>* data() const;
  ChannelBuffer<float>* data_f();
  const ChannelBuffer<float>* data_f() const;

  // Returns a pointer to the ChannelBuffer that encapsulates the split data.
  ChannelBuffer<int16_t>* split_data();
  const ChannelBuffer<int16_t>* split_data() const;
  ChannelBuffer<float>* split_data_f();
  const ChannelBuffer<float>* split_data_f() const;

  // Returns a pointer to the low-pass data downmixed to mono. If this data
  // isn't already available it re-calculates it.
  const int16_t* mixed_low_pass_data();
  const int16_t* low_pass_reference(int channel) const;

  const float* keyboard_data() const;

  void set_activity(AudioFrame::VADActivity activity);
  AudioFrame::VADActivity activity() const;

  // Use for int16 interleaved data.
  void DeinterleaveFrom(AudioFrame* audioFrame);
  // If |data_changed| is false, only the non-audio data members will be copied
  // to |frame|.
  void InterleaveTo(AudioFrame* frame, bool data_changed) const;

  // Use for float deinterleaved data.
  void CopyFrom(const float* const* data,
                int num_frames,
                AudioProcessing::ChannelLayout layout);
  void CopyTo(int num_frames,
              AudioProcessing::ChannelLayout layout,
              float* const* data);
  void CopyLowPassToReference();

  // Splits the signal into different bands.
  void SplitIntoFrequencyBands();
  // Recombine the different bands into one signal.
  void MergeFrequencyBands();

 private:
  // Called from DeinterleaveFrom() and CopyFrom().
  void InitForNewData();

  // The audio is passed into DeinterleaveFrom() or CopyFrom() with input
  // format (samples per channel and number of channels).
  const int input_num_frames_;
  const int num_input_channels_;
  // The audio is stored by DeinterleaveFrom() or CopyFrom() with processing
  // format.
  const int proc_num_frames_;
  const int num_proc_channels_;
  // The audio is returned by InterleaveTo() and CopyTo() with output samples
  // per channels and the current number of channels. This last one can be
  // changed at any time using set_num_channels().
  const int output_num_frames_;
  int num_channels_;

  int num_bands_;
  int num_split_frames_;
  bool mixed_low_pass_valid_;
  bool reference_copied_;
  AudioFrame::VADActivity activity_;

  const float* keyboard_data_;
  rtc::scoped_ptr<IFChannelBuffer> data_;
  rtc::scoped_ptr<IFChannelBuffer> split_data_;
  rtc::scoped_ptr<SplittingFilter> splitting_filter_;
  rtc::scoped_ptr<ChannelBuffer<int16_t> > mixed_low_pass_channels_;
  rtc::scoped_ptr<ChannelBuffer<int16_t> > low_pass_reference_channels_;
  rtc::scoped_ptr<ChannelBuffer<float> > input_buffer_;
  rtc::scoped_ptr<ChannelBuffer<float> > process_buffer_;
  ScopedVector<PushSincResampler> input_resamplers_;
  ScopedVector<PushSincResampler> output_resamplers_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AUDIO_BUFFER_H_
