/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_POLE_ZERO_FILTER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_POLE_ZERO_FILTER_H_

#include "webrtc/typedefs.h"

namespace webrtc {

class PoleZeroFilter {
 public:
  ~PoleZeroFilter() {}

  static PoleZeroFilter* Create(const float* numerator_coefficients,
                                int order_numerator,
                                const float* denominator_coefficients,
                                int order_denominator);

  int Filter(const int16_t* in, int num_input_samples, float* output);

 private:
  PoleZeroFilter(const float* numerator_coefficients,
                 int order_numerator,
                 const float* denominator_coefficients,
                 int order_denominator);

  static const int kMaxFilterOrder = 24;

  int16_t past_input_[kMaxFilterOrder * 2];
  float past_output_[kMaxFilterOrder * 2];

  float numerator_coefficients_[kMaxFilterOrder + 1];
  float denominator_coefficients_[kMaxFilterOrder + 1];

  int order_numerator_;
  int order_denominator_;
  int highest_order_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_POLE_ZERO_FILTER_H_
