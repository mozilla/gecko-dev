/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_OVERUSE_DETECTOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_OVERUSE_DETECTOR_H_

#include <list>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {
enum RateControlRegion;

class OveruseDetector {
 public:
  explicit OveruseDetector(const OverUseDetectorOptions& options);
  ~OveruseDetector();

  // Update the detection state based on the estimated inter-arrival time delta
  // offset. |timestamp_delta| is the delta between the last timestamp which the
  // estimated offset is based on and the last timestamp on which the last
  // offset was based on, representing the time between detector updates.
  // |num_of_deltas| is the number of deltas the offset estimate is based on.
  // Returns the state after the detection update.
  BandwidthUsage Detect(double offset, double timestamp_delta,
                        int num_of_deltas);

  // Returns the current detector state.
  BandwidthUsage State() const;

  // Sets the current rate-control region as decided by RemoteRateControl. This
  // affects the sensitivity of the detector.
  void SetRateControlRegion(webrtc::RateControlRegion region);

 private:
  // Must be first member variable. Cannot be const because we need to be
  // copyable.
  webrtc::OverUseDetectorOptions options_;
  double threshold_;
  double prev_offset_;
  double time_over_using_;
  int overuse_counter_;
  BandwidthUsage hypothesis_;

  DISALLOW_COPY_AND_ASSIGN(OveruseDetector);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_OVERUSE_DETECTOR_H_
