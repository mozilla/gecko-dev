/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_ACCELERATE_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_ACCELERATE_H_

#include <assert.h>

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"
#include "webrtc/modules/audio_coding/neteq4/time_stretch.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Forward declarations.
class BackgroundNoise;

// This class implements the Accelerate operation. Most of the work is done
// in the base class TimeStretch, which is shared with the PreemptiveExpand
// operation. In the Accelerate class, the operations that are specific to
// Accelerate are implemented.
class Accelerate : public TimeStretch {
 public:
  Accelerate(int sample_rate_hz, size_t num_channels,
             const BackgroundNoise& background_noise)
      : TimeStretch(sample_rate_hz, num_channels, background_noise) {
  }

  virtual ~Accelerate() {}

  // This method performs the actual Accelerate operation. The samples are
  // read from |input|, of length |input_length| elements, and are written to
  // |output|. The number of samples removed through time-stretching is
  // is provided in the output |length_change_samples|. The method returns
  // the outcome of the operation as an enumerator value.
  ReturnCodes Process(const int16_t* input,
                      size_t input_length,
                      AudioMultiVector* output,
                      int16_t* length_change_samples);

 protected:
  // Sets the parameters |best_correlation| and |peak_index| to suitable
  // values when the signal contains no active speech.
  virtual void SetParametersForPassiveSpeech(size_t len,
                                             int16_t* best_correlation,
                                             int* peak_index) const OVERRIDE;

  // Checks the criteria for performing the time-stretching operation and,
  // if possible, performs the time-stretching.
  virtual ReturnCodes CheckCriteriaAndStretch(
      const int16_t* input, size_t input_length, size_t peak_index,
      int16_t best_correlation, bool active_speech,
      AudioMultiVector* output) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(Accelerate);
};

struct AccelerateFactory {
  AccelerateFactory() {}
  virtual ~AccelerateFactory() {}

  virtual Accelerate* Create(int sample_rate_hz,
                             size_t num_channels,
                             const BackgroundNoise& background_noise) const;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_ACCELERATE_H_
