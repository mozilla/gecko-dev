
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

#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr QualityConvergenceMonitor::Parameters kParametersOnlyStaticThreshold =
    {.static_qp_threshold = 13, .dynamic_detection_enabled = false};
constexpr QualityConvergenceMonitor::Parameters
    kParametersWithDynamicDetection = {.static_qp_threshold = 13,
                                       .dynamic_detection_enabled = true,
                                       .recent_window_length = 3,
                                       .past_window_length = 9,
                                       .dynamic_qp_threshold = 24};

// Test the basics of the algorithm.

TEST(QualityConvergenceMonitorAlgorithm, StaticThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersOnlyStaticThreshold;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  for (bool is_refresh_frame : {false, true}) {
    // Ramp down from 100. Not at target quality until qp <= static threshold.
    for (int qp = 100; qp > p.static_qp_threshold; --qp) {
      monitor->AddSample(qp, is_refresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
    }

    monitor->AddSample(p.static_qp_threshold, is_refresh_frame);
    EXPECT_TRUE(monitor->AtTargetQuality());

    // 100 samples just above the threshold is not at target quality.
    for (int i = 0; i < 100; ++i) {
      monitor->AddSample(p.static_qp_threshold + 1, is_refresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
    }
  }
}

TEST(QualityConvergenceMonitorAlgorithm,
     StaticThresholdWithDynamicDetectionEnabled) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  for (bool is_refresh_frame : {false, true}) {
    // Clear buffer.
    monitor->AddSample(-1, /*is_refresh_frame=*/false);
    EXPECT_FALSE(monitor->AtTargetQuality());

    // Ramp down from 100. Not at target quality until qp <= static threshold.
    for (int qp = 100; qp > p.static_qp_threshold; --qp) {
      monitor->AddSample(qp, is_refresh_frame);
      EXPECT_FALSE(monitor->AtTargetQuality());
    }

    // A single frame at the static QP threshold is considered to be at target
    // quality regardless of if it's a refresh frame or not.
    monitor->AddSample(p.static_qp_threshold, is_refresh_frame);
    EXPECT_TRUE(monitor->AtTargetQuality());
  }

  // 100 samples just above the threshold is not at target quality if it's not a
  // refresh frame.
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(p.static_qp_threshold + 1, /*is_refresh_frame=*/false);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
}

TEST(QualityConvergenceMonitorAlgorithm, ConvergenceAtDynamicThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // `recent_window_length` + `past_window_length` refresh frames at the dynamic
  // threshold must mean we're at target quality.
  for (size_t i = 0; i < p.recent_window_length + p.past_window_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold, /*is_refresh_frame=*/true);
  }
  EXPECT_TRUE(monitor->AtTargetQuality());
}

TEST(QualityConvergenceMonitorAlgorithm, NoConvergenceAboveDynamicThreshold) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // 100 samples just above the threshold must imply that we're not at target
  // quality.
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold + 1, /*is_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
}

TEST(QualityConvergenceMonitorAlgorithm,
     MaintainAtTargetQualityForRefreshFrames) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // `recent_window_length` + `past_window_length` refresh frames at the dynamic
  // threshold must mean we're at target quality.
  for (size_t i = 0; i < p.recent_window_length + p.past_window_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold, /*is_refresh_frame=*/true);
  }
  EXPECT_TRUE(monitor->AtTargetQuality());

  int qp = p.dynamic_qp_threshold;
  for (int i = 0; i < 100; ++i) {
    monitor->AddSample(qp++, /*is_refresh_frame=*/true);
    EXPECT_TRUE(monitor->AtTargetQuality());
  }

  // Reset state for first frame that is not a refresh frame.
  monitor->AddSample(qp, /*is_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());
}

// Test corner cases.

TEST(QualityConvergenceMonitorAlgorithm, SufficientData) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);
  ASSERT_TRUE(monitor);

  // Less than `recent_window_length + 1` refresh frame QP values at the dynamic
  // threshold is not sufficient.
  for (size_t i = 0; i < p.recent_window_length; ++i) {
    monitor->AddSample(p.dynamic_qp_threshold, /*is_refresh_frame=*/true);
    // Not sufficient data
    EXPECT_FALSE(monitor->AtTargetQuality());
  }

  // However, `recent_window_length + 1` QP values are sufficient.
  monitor->AddSample(p.dynamic_qp_threshold, /*is_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());
}

TEST(QualityConvergenceMonitorAlgorithm,
     AtTargetIfQpPastLessThanOrEqualToQpRecent) {
  QualityConvergenceMonitor::Parameters p = kParametersWithDynamicDetection;
  p.past_window_length = 3;
  p.recent_window_length = 3;
  auto monitor = std::make_unique<QualityConvergenceMonitor>(p);

  // Sequence for which QP_past > QP_recent.
  for (int qp : {23, 21, 21, 21, 21, 22}) {
    monitor->AddSample(qp, /*is_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }

  // Reset QP window.
  monitor->AddSample(-1, /*is_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());

  // Sequence for which one additional sample of 22 will make QP_past ==
  // QP_recent.
  for (int qp : {22, 21, 21, 21, 21}) {
    monitor->AddSample(qp, /*is_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
  monitor->AddSample(22, /*is_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());

  // Reset QP window.
  monitor->AddSample(-1, /*is_refresh_frame=*/false);
  EXPECT_FALSE(monitor->AtTargetQuality());

  // Sequence for which one additional sample of 23 will make QP_past <
  // QP_recent.
  for (int qp : {22, 21, 21, 21, 21}) {
    monitor->AddSample(qp, /*is_refresh_frame=*/true);
    EXPECT_FALSE(monitor->AtTargetQuality());
  }
  monitor->AddSample(23, /*is_refresh_frame=*/true);
  EXPECT_TRUE(monitor->AtTargetQuality());
}

}  // namespace
}  // namespace webrtc
