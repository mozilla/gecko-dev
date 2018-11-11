/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioFrame;
class AgcAudioProc;
class Histogram;
class PitchBasedVad;
class Resampler;
class StandaloneVad;

class Agc {
 public:
  Agc();
  virtual ~Agc();

  // Returns the proportion of samples in the buffer which are at full-scale
  // (and presumably clipped).
  virtual float AnalyzePreproc(const int16_t* audio, int length);
  // |audio| must be mono; in a multi-channel stream, provide the first (usually
  // left) channel.
  virtual int Process(const int16_t* audio, int length, int sample_rate_hz);

  // Retrieves the difference between the target RMS level and the current
  // signal RMS level in dB. Returns true if an update is available and false
  // otherwise, in which case |error| should be ignored and no action taken.
  virtual bool GetRmsErrorDb(int* error);
  virtual void Reset();

  virtual int set_target_level_dbfs(int level);
  virtual int target_level_dbfs() const { return target_level_dbfs_; }

  virtual void EnableStandaloneVad(bool enable);
  virtual bool standalone_vad_enabled() const {
    return standalone_vad_enabled_;
  }

  virtual double voice_probability() const { return last_voice_probability_; }

 private:
  double target_level_loudness_;
  double last_voice_probability_;
  int target_level_dbfs_;
  bool standalone_vad_enabled_;
  rtc::scoped_ptr<Histogram> histogram_;
  rtc::scoped_ptr<Histogram> inactive_histogram_;
  rtc::scoped_ptr<AgcAudioProc> audio_processing_;
  rtc::scoped_ptr<PitchBasedVad> pitch_based_vad_;
  rtc::scoped_ptr<StandaloneVad> standalone_vad_;
  rtc::scoped_ptr<Resampler> resampler_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_H_
