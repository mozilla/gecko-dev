/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_
#define DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_

#include "TimeUnits.h"
#include "mozilla/RollingMean.h"

#include <algorithm>
#include <cstdint>

#include "MediaSegment.h"

namespace mozilla {

/**
 * DriftController calculates the divergence of the source clock from its
 * nominal (provided) rate compared to that of the target clock, which drives
 * the calculations.
 *
 * The DriftController looks at how the current buffering level differs from the
 * desired buffering level and sets a corrected source rate. A resampler should
 * be configured to resample from the corrected source rate to the nominal
 * target rate. It assumes that the resampler is initially configured to
 * resample from the nominal source rate to the nominal target rate.
 *
 * The pref `media.clock drift.buffering` can be used to configure the minimum
 * initial desired internal buffering. Right now it is at 50ms. A larger desired
 * buffering level will be used if deemed necessary based on input device
 * latency, reported or observed. It will also be increased as a response to an
 * underrun, since that indicates the buffer was too small.
 */
class DriftController final {
 public:
  /**
   * Provide the nominal source and the target sample rate.
   */
  DriftController(uint32_t aSourceRate, uint32_t aTargetRate,
                  media::TimeUnit aDesiredBuffering);

  /**
   * Set the buffering level that the controller should target.
   */
  void SetDesiredBuffering(media::TimeUnit aDesiredBuffering);

  /**
   * Reset internal PID-controller state in a way that is suitable for handling
   * an underrun.
   */
  void ResetAfterUnderrun();

  /**
   * Returns the drift-corrected source rate.
   */
  uint32_t GetCorrectedSourceRate() const;

  /**
   * The number of times mCorrectedSourceRate has been changed to adjust to
   * drift.
   */
  uint32_t NumCorrectionChanges() const { return mNumCorrectionChanges; }

  /**
   * The amount of time that the difference between the buffering level and
   * the desired value has been both less than 20% of the desired level and
   * less than 10ms of buffered frames.
   */
  media::TimeUnit DurationNearDesired() const { return mDurationNearDesired; }

  /**
   * The amount of time that has passed since the last time SetDesiredBuffering
   * was called.
   */
  media::TimeUnit DurationSinceDesiredBufferingChange() const {
    return mTotalTargetClock - mLastDesiredBufferingChangeTime;
  }

  /**
   * A rolling window average measurement of source latency by looking at the
   * duration of the source buffer.
   */
  media::TimeUnit MeasuredSourceLatency() const {
    return mMeasuredSourceLatency.mean();
  }

  /**
   * Update the available source frames, target frames, and the current
   * buffer, in every iteration. If the conditions are met a new correction is
   * calculated. A new correction is calculated every mAdjustmentInterval. In
   * addition to that, the correction is clamped so that the output sample rate
   * changes by at most 0.1% of its nominal rate each correction.
   */
  void UpdateClock(media::TimeUnit aSourceDuration,
                   media::TimeUnit aTargetDuration, uint32_t aBufferedFrames,
                   uint32_t aBufferSize);

 private:
  // This implements a simple PID controller with feedback.
  // Set point:     SP = mDesiredBuffering.
  // Process value: PV(t) = mAvgBufferedFramesEst. This includes the feedback.
  // Error:         e(t) = mAvgBufferedFramesEst - mDesiredBuffering.
  //                Error is positive when the process value is high, which is
  //                the opposite of conventional PID controllers because this
  //                is a reverse-acting system.
  // Control value: CV(t) = the value to add to the nominal source rate, i.e.
  //                the corrected source rate = nominal source rate + CV(t).
  //
  // Controller:
  // Proportional part: The error, p(t) = e(t), multiplied by a gain factor, Kp.
  // Integral part:     The historic cumulative value of the error,
  //                    i(t+1) = i(t) + e(t+1), multiplied by a gain factor, Ki.
  // Derivative part:   The error's rate of change, d(t+1) = (e(t+1)-e(t))/1,
  //                    multiplied by a gain factor, Kd.
  // Control signal:    The sum of the parts' output,
  //                    u(t) = Kp*p(t) + Ki*i(t) + Kd*d(t).
  //
  // Control action: Converting the control signal to a source sample rate.
  //                 Simplified, a positive control signal means the buffer is
  //                 higher than desired (because the error is positive),
  //                 so the source sample rate must be increased in order to
  //                 consume input data faster.
  //                 We calculate the corrected source rate by simply adding
  //                 the control signal, u(t), to the nominal source rate.
  //
  // Hysteresis: As long as the error is within a threshold of 20% of the set
  //             point (desired buffering level) (up to 10ms for >50ms desired
  //             buffering), we call this the hysteresis threshold, the control
  //             signal does not influence the corrected target rate at all.
  //             This is to reduce the frequency at which we need to reconfigure
  //             the resampler, as it causes some allocations.
  void CalculateCorrection(uint32_t aBufferedFrames, uint32_t aBufferSize);

 public:
  const uint8_t mPlotId;
  const uint32_t mSourceRate;
  const uint32_t mTargetRate;
  const media::TimeUnit mAdjustmentInterval = media::TimeUnit::FromSeconds(1);
  const media::TimeUnit mIntegralCapTimeLimit =
      media::TimeUnit(10, 1).ToBase(mTargetRate);

 private:
  media::TimeUnit mDesiredBuffering;
  float mPreviousError = 0.f;
  float mIntegral = 0.0;
  Maybe<float> mIntegralCenterForCap;
  float mCorrectedSourceRate;
  Maybe<int32_t> mLastHysteresisBoundaryCorrection;
  media::TimeUnit mDurationWithinHysteresis;
  media::TimeUnit mDurationNearDesired;
  uint32_t mNumCorrectionChanges = 0;
  // Moving averages of input and output durations, used in a ratio to
  // estimate clock drift. Each average is calculated using packet durations
  // from the same time intervals (between output requests), with the same
  // weights, to support their use as a ratio.  Durations from many packets
  // are essentially summed (with consistent denominators) to provide
  // longish-term measures of clock advance.  These are independent of any
  // corrections in resampling ratio.
  double mInputDurationAvg = 0.0;
  double mOutputDurationAvg = 0.0;
  // Moving average of mInputDurationAvg/mOutputDurationAvg to smooth
  // out short-term deviations from an estimated longish-term drift rate.
  // Greater than 1 means the input clock has advanced faster than the output
  // clock.  This is the output of a second low pass filter stage.
  double mDriftEstimate = 1.0;
  // Output of the first low pass filter stage for mDriftEstimate
  double mStage1Drift = 1.0;
  // Estimate of the average buffering level after each output request, in
  // input frames (and fractions thereof), smoothed to reduce the effect of
  // short term variations.  This is adjusted for estimated clock drift and for
  // corrections in the resampling ratio.  This is the output of a second low
  // pass filter stage.
  double mAvgBufferedFramesEst = 0.0;
  // Output of the first low pass filter stage for mAvgBufferedFramesEst
  double mStage1Buffered = 0.0;
  // Whether handling an underrun, including waiting for the first input sample.
  bool mIsHandlingUnderrun = true;
  // An estimate of the source's latency, i.e. callback buffer size, in frames.
  // Like mInputDurationAvg, this measures the duration arriving between each
  // output request, but mMeasuredSourceLatency does not include zero
  // duration measurements.
  RollingMean<media::TimeUnit, media::TimeUnit> mMeasuredSourceLatency;
  // An estimate of the target's latency, i.e. callback buffer size, in frames.
  RollingMean<media::TimeUnit, media::TimeUnit> mMeasuredTargetLatency;

  media::TimeUnit mTargetClock;
  media::TimeUnit mTotalTargetClock;
  media::TimeUnit mTargetClockAfterLastSourcePacket;
  media::TimeUnit mLastDesiredBufferingChangeTime;
};

}  // namespace mozilla
#endif  // DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_
