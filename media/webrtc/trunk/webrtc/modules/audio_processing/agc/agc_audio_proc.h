/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_AUDIO_PROC_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_AUDIO_PROC_H_

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_processing/agc/common.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioFrame;
class PoleZeroFilter;

class AgcAudioProc {
 public:
  // Forward declare iSAC structs.
  struct PitchAnalysisStruct;
  struct PreFiltBankstr;

  AgcAudioProc();
  ~AgcAudioProc();

  int ExtractFeatures(const int16_t* audio_frame,
                      int length,
                      AudioFeatures* audio_features);

  static const int kDftSize = 512;

 private:
  void PitchAnalysis(double* pitch_gains, double* pitch_lags_hz, int length);
  void SubframeCorrelation(double* corr, int lenght_corr, int subframe_index);
  void GetLpcPolynomials(double* lpc, int length_lpc);
  void FindFirstSpectralPeaks(double* f_peak, int length_f_peak);
  void Rms(double* rms, int length_rms);
  void ResetBuffer();

  // To compute spectral peak we perform LPC analysis to get spectral envelope.
  // For every 30 ms we compute 3 spectral peak there for 3 LPC analysis.
  // LPC is computed over 15 ms of windowed audio. For every 10 ms sub-frame
  // we need 5 ms of past signal to create the input of LPC analysis.
  static const int kNumPastSignalSamples = kSampleRateHz / 200;

  // TODO(turajs): maybe defining this at a higher level (maybe enum) so that
  // all the code recognize it as "no-error."
  static const int kNoError = 0;

  static const int kNum10msSubframes = 3;
  static const int kNumSubframeSamples = kSampleRateHz / 100;
  static const int kNumSamplesToProcess = kNum10msSubframes *
      kNumSubframeSamples;  // Samples in 30 ms @ given sampling rate.
  static const int kBufferLength = kNumPastSignalSamples + kNumSamplesToProcess;
  static const int kIpLength = kDftSize >> 1;
  static const int kWLength = kDftSize >> 1;

  static const int kLpcOrder = 16;

  int ip_[kIpLength];
  float w_fft_[kWLength];

  // A buffer of 5 ms (past audio) + 30 ms (one iSAC frame ).
  float audio_buffer_[kBufferLength];
  int num_buffer_samples_;

  double log_old_gain_;
  double old_lag_;

  rtc::scoped_ptr<PitchAnalysisStruct> pitch_analysis_handle_;
  rtc::scoped_ptr<PreFiltBankstr> pre_filter_handle_;
  rtc::scoped_ptr<PoleZeroFilter> high_pass_filter_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_AGC_AUDIO_PROC_H_
