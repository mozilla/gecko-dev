/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_convergence_monitor.h"

#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {

QualityConvergenceMonitor::QualityConvergenceMonitor(const Parameters& params)
    : params_(params) {
  RTC_CHECK(
      !params_.dynamic_detection_enabled ||
      (params_.past_window_length > 0 && params_.recent_window_length > 0));
}

// Adds the sample to the algorithms detection window and runs the following
// convergence detection algorithm to determine if the time series of QP
// values indicates that the encoded video has reached "target quality".
//
// Definitions
//
// - Let x[n] be the pixel data of a video frame.
// - Let e[n] be the encoded representation of x[n].
// - Let qp[n] be the corresponding QP value of the encoded video frame e[n].
// - x[n] is a refresh frame if x[n] = x[n-1].
// - qp_window is a list (or queue) of stored QP values, with size
//   L <= past_window_length + recent_window_length.
// - qp_window can be partioned into:
//     qp_past = qp_window[ 0:end-recent_window_length ] and
//     qp_recent = qp_window[ -recent_window_length:end ].
// - Let dynamic_qp_threshold be a maximum QP value for which convergence
//   is accepted.
//
// Algorithm
//
// For each encoded video frame e[n], take the corresponding qp[n] and do the
// following:
// 0. Check Static Threshold: if qp[n] < static_qp_threshold, return true.
// 1. Check for Refresh Frame: If x[n] is not a refresh frame:
//     - Clear Q.
//     - Return false.
// 2. Check Previous Convergence: If x[n] is a refresh frame AND true was
//    returned for x[n-1], return true.
// 3. Update QP History: Append qp[n] to qp_window. If qp_window's length
//    exceeds past_window_length + recent_window_length, remove the first
//    element.
// 4. Check for Sufficient Data: If L <= recent_window_length, return false.
// 5. Calculate Average QP: Calculate avg(qp_past) and avg(ap_recent).
// 6. Determine Convergence: If avg(qp_past) <= dynamic_qp_threshold AND
//    avg(qp_past) <= avg(qp_recent), return true. Otherwise, return false.
//
void QualityConvergenceMonitor::AddSample(int qp, bool is_refresh_frame) {
  // Invalid QP.
  if (qp < 0) {
    qp_window_.clear();
    at_target_quality_ = false;
    return;
  }

  // 0. Check static threshold.
  if (qp <= params_.static_qp_threshold) {
    at_target_quality_ = true;
    return;
  }

  // 1. Check for refresh frame and if dynamic detection is disabled.
  if (!is_refresh_frame || !params_.dynamic_detection_enabled) {
    qp_window_.clear();
    at_target_quality_ = false;
    return;
  }

  // 2. Check previous convergence.
  RTC_CHECK(is_refresh_frame);
  if (at_target_quality_) {
    // No need to update state.
    return;
  }

  // 3. Update QP history.
  qp_window_.push_back(qp);
  if (qp_window_.size() >
      params_.recent_window_length + params_.past_window_length) {
    qp_window_.pop_front();
  }

  // 4. Check for sufficient data.
  if (qp_window_.size() <= params_.recent_window_length) {
    // No need to update state.
    RTC_CHECK(at_target_quality_ == false);
    return;
  }

  // 5. Calculate average QP.
  float qp_past_average =
      std::accumulate(qp_window_.begin(),
                      qp_window_.end() - params_.recent_window_length, 0.0) /
      (qp_window_.size() - params_.recent_window_length);
  float qp_recent_average =
      std::accumulate(qp_window_.end() - params_.recent_window_length,
                      qp_window_.end(), 0.0) /
      params_.recent_window_length;
  // 6. Determine convergence.
  if (qp_past_average <= params_.dynamic_qp_threshold &&
      qp_past_average <= qp_recent_average) {
    at_target_quality_ = true;
  }
}

bool QualityConvergenceMonitor::AtTargetQuality() const {
  return at_target_quality_;
}

}  // namespace webrtc
