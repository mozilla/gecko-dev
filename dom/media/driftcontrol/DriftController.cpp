/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DriftController.h"

#include <atomic>
#include <cmath>
#include <mutex>

#include "mozilla/CheckedInt.h"
#include "mozilla/Logging.h"

namespace mozilla {

LazyLogModule gDriftControllerGraphsLog("DriftControllerGraphs");
extern LazyLogModule gMediaTrackGraphLog;

#define LOG_CONTROLLER(level, controller, format, ...)             \
  MOZ_LOG(gMediaTrackGraphLog, level,                              \
          ("DriftController %p: (plot-id %u) " format, controller, \
           (controller)->mPlotId, ##__VA_ARGS__))
#define LOG_PLOT_NAMES()                                                     \
  MOZ_LOG(                                                                   \
      gDriftControllerGraphsLog, LogLevel::Verbose,                          \
      ("id,t,buffering,avgbuffered,desired,buffersize,inlatency,outlatency," \
       "inframesavg,outframesavg,inrate,outrate,driftestimate,"              \
       "hysteresisthreshold,corrected,hysteresiscorrected,configured,"       \
       "p,d,kpp,kdd,control"))
#define LOG_PLOT_VALUES(id, t, buffering, avgbuffered, desired, buffersize,    \
                        inlatency, outlatency, inframesavg, outframesavg,      \
                        inrate, outrate, driftestimate, hysteresisthreshold,   \
                        corrected, hysteresiscorrected, configured, p, d, kpp, \
                        kdd, control)                                          \
  MOZ_LOG(gDriftControllerGraphsLog, LogLevel::Verbose,                        \
          ("DriftController %u,%.3f,%u,%.5f,%" PRId64 ",%u,%" PRId64 ","       \
           "%" PRId64 ",%.5f,%.5f,%u,%u,"                                      \
           "%.9f,%" PRId64 ",%.5f,%.5f,"                                       \
           "%ld,%.5f,%.5f,%.5f,%.5f,%.5f",                                     \
           id, t, buffering, avgbuffered, desired, buffersize, inlatency,      \
           outlatency, inframesavg, outframesavg, inrate, outrate,             \
           driftestimate, hysteresisthreshold, corrected, hysteresiscorrected, \
           configured, p, d, kpp, kdd, control))

static uint8_t GenerateId() {
  static std::atomic<uint8_t> id{0};
  return ++id;
}

DriftController::DriftController(uint32_t aSourceRate, uint32_t aTargetRate,
                                 media::TimeUnit aDesiredBuffering)
    : mPlotId(GenerateId()),
      mSourceRate(aSourceRate),
      mTargetRate(aTargetRate),
      mDesiredBuffering(aDesiredBuffering),
      mCorrectedSourceRate(static_cast<float>(aSourceRate)),
      mMeasuredSourceLatency(5),
      mMeasuredTargetLatency(5) {
  LOG_CONTROLLER(
      LogLevel::Info, this,
      "Created. Resampling %uHz->%uHz. Initial desired buffering: %.2fms.",
      mSourceRate, mTargetRate, mDesiredBuffering.ToSeconds() * 1000.0);
  static std::once_flag sOnceFlag;
  std::call_once(sOnceFlag, [] { LOG_PLOT_NAMES(); });
}

void DriftController::SetDesiredBuffering(media::TimeUnit aDesiredBuffering) {
  LOG_CONTROLLER(LogLevel::Debug, this, "SetDesiredBuffering %.2fms->%.2fms",
                 mDesiredBuffering.ToSeconds() * 1000.0,
                 aDesiredBuffering.ToSeconds() * 1000.0);
  mLastDesiredBufferingChangeTime = mTotalTargetClock;
  mDesiredBuffering = aDesiredBuffering.ToBase(mSourceRate);
}

void DriftController::ResetAfterUnderrun() {
  mIsHandlingUnderrun = true;
  mPreviousError = 0.0;
  // Trigger a recalculation on the next clock update.
  mTargetClock = mAdjustmentInterval;
}

uint32_t DriftController::GetCorrectedSourceRate() const {
  return std::lround(mCorrectedSourceRate);
}

void DriftController::UpdateClock(media::TimeUnit aSourceDuration,
                                  media::TimeUnit aTargetDuration,
                                  uint32_t aBufferedFrames,
                                  uint32_t aBufferSize) {
  MOZ_ASSERT(!aTargetDuration.IsZero());

  mTargetClock += aTargetDuration;
  mTotalTargetClock += aTargetDuration;

  mMeasuredTargetLatency.insert(aTargetDuration);

  if (aSourceDuration.IsZero()) {
    // Only update after having received input, so that controller input,
    // packet sizes and buffering measurements, are more stable when the input
    // stream's callback interval is much larger than that of the output
    // stream.  The buffer level is therefore sampled at high points (rather
    // than being an average of all points), which is consistent with the
    // desired level of pre-buffering set on the DynamicResampler only after
    // an input packet has recently arrived.  There is some symmetry with
    // output durations, which are similarly never zero: the buffer level is
    // sampled at the lesser of input and output callback rates.
    return;
  }

  media::TimeUnit targetDuration =
      mTotalTargetClock - mTargetClockAfterLastSourcePacket;
  mTargetClockAfterLastSourcePacket = mTotalTargetClock;

  mMeasuredSourceLatency.insert(aSourceDuration);

  double sourceDurationSecs = aSourceDuration.ToSeconds();
  double targetDurationSecs = targetDuration.ToSeconds();
  if (mOutputDurationAvg == 0.0) {
    // Initialize the packet duration moving averages with equal values for an
    // initial estimate of zero clock drift.  When the input packets are much
    // larger than output packets, targetDurationSecs may initially be much
    // smaller.  Use the maximum for a better estimate of the average output
    // duration per input packet (or average input duration per output packet
    // if input packets are smaller than output packets).
    mInputDurationAvg = mOutputDurationAvg =
        std::max(sourceDurationSecs, targetDurationSecs);
  }
  // UpdateAverageWithMeasurement() implements an exponential moving average
  // with a weight small enough so that the influence of short term variations
  // is small, but not so small that response time is delayed more than
  // necessary.
  //
  // For the packet duration averages, a constant weight means that the moving
  // averages behave similarly to sums of durations, and so can be used in a
  // ratio for the drift estimate.  Input arriving shortly before or after
  // an UpdateClock() call, in response to an output request, is weighted
  // similarly.
  //
  // For 10 ms packet durations, a weight of 0.01 corresponds to a time
  // constant of about 1 second (the time over which the effect of old data
  // points attenuates with a factor of exp(-1)).
  auto UpdateAverageWithMeasurement = [](double* aAvg, double aData) {
    constexpr double kMovingAvgWeight = 0.01;
    *aAvg += kMovingAvgWeight * (aData - *aAvg);
  };
  UpdateAverageWithMeasurement(&mInputDurationAvg, sourceDurationSecs);
  UpdateAverageWithMeasurement(&mOutputDurationAvg, targetDurationSecs);
  double driftEstimate = mInputDurationAvg / mOutputDurationAvg;
  // The driftEstimate is susceptible to changes in the input packet timing or
  // duration, so use exponential smoothing to reduce the effect of short term
  // variations. Apply a cascade of two exponential smoothing filters, which
  // is a second order low pass filter, which attenuates high frequency
  // components better than a single first order filter with the same total
  // time constant. The attenuations of multiple filters are multiplicative
  // while the time constants are only additive.
  UpdateAverageWithMeasurement(&mStage1Drift, driftEstimate);
  UpdateAverageWithMeasurement(&mDriftEstimate, mStage1Drift);
  // Adjust the average buffer level estimates for drift and for the
  // correction that was applied with this output packet, so that it still
  // provides an estimate of the average buffer level.
  double adjustment = targetDurationSecs *
                      (mSourceRate * mDriftEstimate - GetCorrectedSourceRate());
  mStage1Buffered += adjustment;
  mAvgBufferedFramesEst += adjustment;
  // Include the current buffer level as a data point in the average buffer
  // level estimate.
  UpdateAverageWithMeasurement(&mStage1Buffered, aBufferedFrames);
  UpdateAverageWithMeasurement(&mAvgBufferedFramesEst, mStage1Buffered);

  if (mIsHandlingUnderrun) {
    mIsHandlingUnderrun = false;
    // Underrun handling invalidates the average buffer level estimate
    // because silent input frames are inserted.  Reset the estimate.
    // This reset also performs the initial estimate when no previous
    // input packets have been received.
    mAvgBufferedFramesEst =
        static_cast<double>(mDesiredBuffering.ToTicksAtRate(mSourceRate));
    mStage1Buffered = mAvgBufferedFramesEst;
  }

  if (mTargetClock >= mAdjustmentInterval) {
    // The adjustment interval has passed. Recalculate.
    CalculateCorrection(aBufferedFrames, aBufferSize);
  }
}

void DriftController::CalculateCorrection(uint32_t aBufferedFrames,
                                          uint32_t aBufferSize) {
  static constexpr float kProportionalGain = 0.07;
  static constexpr float kDerivativeGain = 0.12;

  // Maximum 0.1% change per update.
  const float cap = static_cast<float>(mSourceRate) / 1000.0f;

  // Resampler source rate that is expected to maintain a constant average
  // buffering level.
  float steadyStateRate =
      static_cast<float>(mDriftEstimate) * static_cast<float>(mSourceRate);
  // Use nominal (not corrected) source rate when interpreting desired
  // buffering so that the set point is independent of the control value.
  uint32_t desiredBufferedFrames = mDesiredBuffering.ToTicksAtRate(mSourceRate);
  int32_t error =
      (CheckedInt32(aBufferedFrames) - desiredBufferedFrames).value();
  float avgError = static_cast<float>(mAvgBufferedFramesEst) -
                   static_cast<float>(desiredBufferedFrames);
  float proportional = avgError;
  // targetClockSec is the number of target clock seconds since last
  // correction.
  float targetClockSec = static_cast<float>(mTargetClock.ToSeconds());
  float derivative = (avgError - mPreviousError) / targetClockSec;
  float controlSignal =
      kProportionalGain * proportional + kDerivativeGain * derivative;
  float correctedRate =
      std::clamp(steadyStateRate + controlSignal, mCorrectedSourceRate - cap,
                 mCorrectedSourceRate + cap);

  // mDesiredBuffering is divided by this to calculate a maximum error that
  // would be considered "near" desired buffering. A denominator of 5
  // corresponds to an error of +/- 20% of the desired buffering.
  static constexpr uint32_t kNearDenominator = 5;  // +/- 20%

  // +/- 10ms band maximum half-width.
  const media::TimeUnit nearCap = media::TimeUnit::FromSeconds(0.01);

  // For the minimum desired buffering of 10ms we have a "near" error band
  // of +/- 2ms (20%). This goes up to +/- 10ms (clamped) at most for when the
  // desired buffering is 50 ms or higher. AudioDriftCorrection uses this
  // threshold when deciding whether to reduce buffering.
  const auto nearThreshold =
      std::min(nearCap, mDesiredBuffering / kNearDenominator)
          .ToTicksAtRate(mSourceRate);

  if (std::abs(error) > nearThreshold) {
    // The error is outside a threshold boundary.
    mDurationNearDesired = media::TimeUnit::Zero();
  } else {
    // The error is within the "near" threshold boundaries.
    mDurationNearDesired += mTargetClock;
  };

  // An avgError within the hysteresis threshold will not trigger corrections
  // to the source sample rate.  The hysteresis threshold for avgError is less
  // than the "near" threshold, so that the more variable instantaneous error
  // might remain within the "near" threshold.
  const auto hysteresisThreshold = nearThreshold / 10;

  float hysteresisCorrectedRate = [&] {
    double abserror = std::abs(avgError);
    if (abserror > hysteresisThreshold) {
      // The error is outside a hysteresis threshold boundary.
      mDurationWithinHysteresis = media::TimeUnit::Zero();
      mLastHysteresisBoundaryCorrection = Some(error);
      return correctedRate;
    }

    // The error is within the hysteresis threshold boundaries.
    mDurationWithinHysteresis += mTargetClock;

    // Would prefer std::signbit, but..
    // https://github.com/microsoft/STL/issues/519.
    if (mLastHysteresisBoundaryCorrection &&
        (*mLastHysteresisBoundaryCorrection < 0) != (error < 0) &&
        abserror > hysteresisThreshold * 3 / 10) {
      // The error came from a boundary and just went 30% past the center line
      // (of the distance between center and boundary). Correct now rather
      // than when reaching the opposite boundary, so we have a chance of
      // finding a stable rate.
      mLastHysteresisBoundaryCorrection = Nothing();
      return correctedRate;
    }

    return mCorrectedSourceRate;
  }();

  LOG_CONTROLLER(
      LogLevel::Verbose, this,
      "Recalculating Correction: Nominal: %uHz->%uHz, Corrected: "
      "%.2fHz->%uHz  (diff %.2fHz), error: %.2fms (hysteresisThreshold: "
      "%.2fms), buffering: %.2fms, desired buffering: %.2fms",
      mSourceRate, mTargetRate, hysteresisCorrectedRate, mTargetRate,
      hysteresisCorrectedRate - mCorrectedSourceRate,
      media::TimeUnit(error, mSourceRate).ToSeconds() * 1000.0,
      media::TimeUnit(hysteresisThreshold, mSourceRate).ToSeconds() * 1000.0,
      media::TimeUnit(aBufferedFrames, mSourceRate).ToSeconds() * 1000.0,
      mDesiredBuffering.ToSeconds() * 1000.0);
  LOG_PLOT_VALUES(mPlotId, mTotalTargetClock.ToSeconds(), aBufferedFrames,
                  mAvgBufferedFramesEst,
                  mDesiredBuffering.ToTicksAtRate(mSourceRate), aBufferSize,
                  mMeasuredSourceLatency.mean().ToTicksAtRate(mSourceRate),
                  mMeasuredTargetLatency.mean().ToTicksAtRate(mTargetRate),
                  mInputDurationAvg * mSourceRate,
                  mOutputDurationAvg * mTargetRate, mSourceRate, mTargetRate,
                  mDriftEstimate, hysteresisThreshold, correctedRate,
                  hysteresisCorrectedRate, std::lround(hysteresisCorrectedRate),
                  proportional, derivative, kProportionalGain * proportional,
                  kDerivativeGain * derivative, controlSignal);

  if (std::lround(mCorrectedSourceRate) !=
      std::lround(hysteresisCorrectedRate)) {
    ++mNumCorrectionChanges;
  }

  mPreviousError = avgError;
  mCorrectedSourceRate = std::max(1.f, hysteresisCorrectedRate);

  // Reset the counters to prepare for the next period.
  mTargetClock = media::TimeUnit::Zero();
}
}  // namespace mozilla

#undef LOG_PLOT_VALUES
#undef LOG_PLOT_NAMES
#undef LOG_CONTROLLER
