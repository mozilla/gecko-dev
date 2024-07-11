/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "DriftController.h"
#include "mozilla/Maybe.h"

using namespace mozilla;
using TimeUnit = media::TimeUnit;

// Advance the output by the specified duration, using a calculated input
// packet duration that provides the specified buffering level.
void AdvanceByOutputDuration(TimeUnit* aCurrentBuffered,
                             DriftController* aController,
                             TimeUnit aOutputDuration,
                             uint32_t aNextBufferedInputFrames) {
  uint32_t nominalSourceRate = aController->mSourceRate;
  uint32_t nominalTargetRate = aController->mTargetRate;
  uint32_t correctedRate = aController->GetCorrectedSourceRate();
  // Use a denominator to exactly track (1/nominalTargetRate)ths of
  // durations in seconds of input frames buffered in the resampler.
  *aCurrentBuffered = aCurrentBuffered->ToBase(
      static_cast<int64_t>(nominalSourceRate) * nominalTargetRate);
  // Buffered input frames to feed the output are removed first, so that the
  // number of input frames required can be calculated.  aCurrentBuffered may
  // temporarily become negative.
  *aCurrentBuffered -= aOutputDuration.ToBase(*aCurrentBuffered) *
                       correctedRate / nominalSourceRate;
  // Determine the input duration (aligned to input frames) that would provide
  // the specified buffering level when rounded down to the nearest input
  // frame.
  int64_t currentBufferedInputFrames =
      aCurrentBuffered->ToBase<TimeUnit::FloorPolicy>(nominalSourceRate)
          .ToTicksAtRate(nominalSourceRate);
  TimeUnit inputDuration(
      CheckedInt64(aNextBufferedInputFrames) - currentBufferedInputFrames,
      nominalSourceRate);
  EXPECT_GE(inputDuration.ToTicksAtRate(nominalSourceRate), 0);
  *aCurrentBuffered += inputDuration;
  // The buffer size is not used in the controller logic.
  uint32_t bufferSize = 0;
  aController->UpdateClock(inputDuration, aOutputDuration,
                           aNextBufferedInputFrames, bufferSize);
}

TEST(TestDriftController, Basic)
{
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);
  uint32_t stepsPerSec = 50;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47957u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47957u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48005u);
}

TEST(TestDriftController, BasicResampler)
{
  // This test is equivalent to Basic, but for the output sample rate, so
  // input buffer frame counts should be equal to those in Basic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 24000, currentBuffered);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);
  uint32_t stepsPerSec = 50;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // low
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47957u);

  // high
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47957u);

  // high
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48005u);
}

TEST(TestDriftController, BufferedInput)
{
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);
  uint32_t stepsPerSec = 20;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, 0);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47990u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47971u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47960u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  // Hysteresis keeps the corrected rate the same.
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47960u);
}

TEST(TestDriftController, BufferedInputWithResampling)
{
  // This test is equivalent to BufferedInput, but for the output sample rate,
  // so input buffer frame counts should be equal to those in BufferedInput.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 24000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(24000, 24000);
  uint32_t stepsPerSec = 20;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, 0);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47990u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47971u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47960u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  // Hysteresis keeps the corrected rate the same.
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47960u);
}

TEST(TestDriftController, SmallError)
{
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = buffered - 48;
  constexpr uint32_t bufferedHigh = buffered + 48;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);
  uint32_t stepsPerSec = 25;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
}

TEST(TestDriftController, SmallBufferedFrames)
{
  constexpr uint32_t bufferedLow = 3 * 480;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  media::TimeUnit oneSec = media::TimeUnit::FromSeconds(1);
  uint32_t stepsPerSec = 40;
  media::TimeUnit stepDuration = oneSec / stepsPerSec;

  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  for (uint32_t i = 0; i < stepsPerSec - 1; ++i) {
    c.UpdateClock(stepDuration, stepDuration, bufferedLow, 0);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  c.UpdateClock(stepDuration, stepDuration, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47996U);
}

TEST(TestDriftController, VerySmallBufferedFrames)
{
  uint32_t bufferedLow = 1;
  uint32_t nominalRate = 48000;

  DriftController c(nominalRate, nominalRate, media::TimeUnit::FromSeconds(1));
  EXPECT_EQ(c.GetCorrectedSourceRate(), nominalRate);

  TimeUnit currentBuffered(bufferedLow, 48000);
  media::TimeUnit hundredMillis = media::TimeUnit(100, 1000);
  uint32_t previousCorrected = nominalRate;
  // Perform enough steps (1500 seconds) that the corrected rate can
  // get to its lower bound, without underflowing zero.
  for (uint32_t i = 0; i < 15000; ++i) {
    // The input packet size is reduced each iteration by as much as possible
    // without completely draining the buffer.
    AdvanceByOutputDuration(&currentBuffered, &c, hundredMillis, bufferedLow);
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    EXPECT_LE(correctedRate, previousCorrected) << "for i=" << i;
    EXPECT_GT(correctedRate, 0u) << "for i=" << i;
    previousCorrected = correctedRate;
  }
  // Check that the corrected rate has reached, does not go beyond, and does
  // not bounce off its lower bound.
  EXPECT_EQ(previousCorrected, 1u);
  for (uint32_t i = 15000; i < 15010; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, hundredMillis, bufferedLow);
    EXPECT_EQ(c.GetCorrectedSourceRate(), 1u) << "for i=" << i;
  }
}

TEST(TestDriftController, SmallStepResponse)
{
  // The DriftController is configured with nominal source rate a little less
  // than the actual rate.
  uint32_t nominalTargetRate = 48000;
  uint32_t nominalSourceRate = 48000;
  uint32_t actualSourceRate = 48000 * 1001 / 1000;  // +0.1% drift

  TimeUnit desiredBuffered = TimeUnit::FromSeconds(0.05);  // 50 ms
  DriftController c(nominalSourceRate, nominalTargetRate, desiredBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), nominalSourceRate);

  uint32_t stepsPerSec = 25;
  // Initial buffer level == desired.  Choose a base to exactly track
  // fractions of frames buffered in the resampler.
  TimeUnit buffered = desiredBuffered.ToBase(nominalSourceRate * stepsPerSec);
  media::TimeUnit inputStepDuration(actualSourceRate,
                                    stepsPerSec * nominalSourceRate);
  media::TimeUnit outputStepDuration(nominalTargetRate,
                                     stepsPerSec * nominalTargetRate);

  // Perform enough steps to observe convergence.
  uint32_t iterationCount = 200 /*seconds*/ * stepsPerSec;
  for (uint32_t i = 0; i < iterationCount; ++i) {
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    buffered += TimeUnit(CheckedInt64(actualSourceRate) - correctedRate,
                         stepsPerSec * nominalSourceRate);
    // The buffer size is not used in the controller logic.
    c.UpdateClock(inputStepDuration, outputStepDuration,
                  buffered.ToTicksAtRate(nominalSourceRate), 0);
    if (outputStepDuration * i > TimeUnit::FromSeconds(50) &&
        /* Corrections are performed only once per second. */
        i % stepsPerSec == 0) {
      EXPECT_EQ(c.GetCorrectedSourceRate(), actualSourceRate) << "for i=" << i;
      EXPECT_NEAR(buffered.ToTicksAtRate(nominalSourceRate),
                  desiredBuffered.ToTicksAtRate(nominalSourceRate), 10)
          << "for i=" << i;
    }
  }
}

TEST(TestDriftController, LargeStepResponse)
{
  // The DriftController is configured with nominal source rate much less than
  // the actual rate.  The large difference between nominal and actual
  // produces large PID terms and capping of the change in resampler input
  // rate to nominalRate/1000.  This does not correspond exactly to an
  // expected use case, but tests the stability of the response when changes
  // are capped.
  uint32_t nominalTargetRate = 48000;
  uint32_t nominalSourceRate = 48000 * 7 / 8;
  uint32_t actualSourceRate = 48000;

  TimeUnit desiredBuffered(actualSourceRate * 10, nominalSourceRate);
  DriftController c(nominalSourceRate, nominalTargetRate, desiredBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), nominalSourceRate);

  uint32_t stepsPerSec = 20;
  // Initial buffer level == desired.  Choose a base to exactly track
  // fractions of frames buffered in the resampler.
  TimeUnit buffered = desiredBuffered.ToBase(nominalSourceRate * stepsPerSec);
  media::TimeUnit inputStepDuration(actualSourceRate,
                                    stepsPerSec * nominalSourceRate);
  media::TimeUnit outputStepDuration(nominalTargetRate,
                                     stepsPerSec * nominalTargetRate);

  // Changes in the corrected rate are limited to nominalRate/1000 per second.
  // Perform enough steps to get from nominal to actual source rate and then
  // observe convergence.
  uint32_t iterationCount = 8 * stepsPerSec * 1000 *
                            (actualSourceRate - nominalSourceRate) /
                            nominalSourceRate;
  EXPECT_GT(outputStepDuration * (iterationCount - 1),
            TimeUnit::FromSeconds(1020));
  for (uint32_t i = 0; i < iterationCount; ++i) {
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    buffered += TimeUnit(CheckedInt64(actualSourceRate) - correctedRate,
                         stepsPerSec * nominalSourceRate);
    // The buffer size is not used in the controller logic.
    c.UpdateClock(inputStepDuration, outputStepDuration,
                  buffered.ToTicksAtRate(nominalSourceRate), 0);
    if (outputStepDuration * i > TimeUnit::FromSeconds(1020) &&
        /* Corrections are performed only once per second. */
        i % stepsPerSec == 0) {
      EXPECT_EQ(c.GetCorrectedSourceRate(), actualSourceRate) << "for i=" << i;
      EXPECT_NEAR(buffered.ToTicksAtRate(nominalSourceRate),
                  desiredBuffered.ToTicksAtRate(nominalSourceRate), 10)
          << "for i=" << i;
    }
  }
}
