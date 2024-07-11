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
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47980u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47991u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48030u);
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
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47980u);

  // high
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47991u);

  // high
  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48030u);
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
  // Hysteresis keeps the corrected rate the same.
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47978u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47976u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47978u);
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
  // Hysteresis keeps the corrected rate the same.
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedLow);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47978u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, buffered);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47976u);

  for (uint32_t i = 0; i < stepsPerSec; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, stepDuration, bufferedHigh);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47978u);
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
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47989U);
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
  // Steps are limited to nominalRate/1000 each second.
  // Perform enough steps (1002 seconds) that the corrected rate can
  // get to its lower bound, without underflowing zero.
  for (uint32_t i = 0; i < 10020; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, hundredMillis, bufferedLow);
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    EXPECT_LE(correctedRate, previousCorrected) << "for i=" << i;
    EXPECT_GT(correctedRate, 0u) << "for i=" << i;
    previousCorrected = correctedRate;
  }
  EXPECT_EQ(previousCorrected, 1u);
  for (uint32_t i = 10020; i < 10030; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, hundredMillis, bufferedLow);
    EXPECT_EQ(c.GetCorrectedSourceRate(), 1u) << "for i=" << i;
  }
}
