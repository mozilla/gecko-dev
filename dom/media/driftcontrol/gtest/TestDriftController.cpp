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
  *aCurrentBuffered += inputDuration;
  // The buffer size is not used in the controller logic.
  uint32_t bufferSize = 0;
  aController->UpdateClock(inputDuration, aOutputDuration,
                           aNextBufferedInputFrames, bufferSize);
}

TEST(TestDriftController, Basic)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BasicResampler)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 24000, currentBuffered);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // low
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  // high
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // high
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BufferedInput)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, BufferedInputWithResampling)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = 3 * 480;
  constexpr uint32_t bufferedHigh = 7 * 480;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 24000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(24000, 24000);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // 0 buffered when updating correction
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48048u);
}

TEST(TestDriftController, SmallError)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t buffered = 5 * 480;
  constexpr uint32_t bufferedLow = buffered - 48;
  constexpr uint32_t bufferedHigh = buffered + 48;

  TimeUnit currentBuffered(buffered, 48000);
  DriftController c(48000, 48000, currentBuffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  // The adjustment interval is 1s.
  const auto oneSec = media::TimeUnit(48000, 48000);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, buffered);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);

  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedHigh);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000u);
}

TEST(TestDriftController, SmallBufferedFrames)
{
  // The buffer level is the only input to the controller logic.
  constexpr uint32_t bufferedLow = 3 * 480;

  DriftController c(48000, 48000, media::TimeUnit::FromSeconds(0.05));
  media::TimeUnit oneSec = media::TimeUnit::FromSeconds(1);
  media::TimeUnit hundredMillis = oneSec / 10;

  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  for (uint32_t i = 0; i < 9; ++i) {
    c.UpdateClock(hundredMillis, hundredMillis, bufferedLow, 0);
  }
  EXPECT_EQ(c.GetCorrectedSourceRate(), 48000U);
  c.UpdateClock(hundredMillis, hundredMillis, bufferedLow, 0);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 47952U);
}

TEST(TestDriftController, VerySmallBufferedFrames)
{
  // The buffer level is the only input to the controller logic.
  uint32_t bufferedLow = 1;
  uint32_t nominalRate = 48000;

  DriftController c(nominalRate, nominalRate, media::TimeUnit::FromSeconds(1));
  EXPECT_EQ(c.GetCorrectedSourceRate(), nominalRate);

  TimeUnit currentBuffered(bufferedLow, 48000);
  media::TimeUnit oneSec = media::TimeUnit::FromSeconds(1);
  uint32_t previousCorrected = nominalRate;
  // Steps are limited to nominalRate/1000.
  // Perform 1001 steps to check the corrected rate does not underflow zero.
  for (uint32_t i = 0; i < 1001; ++i) {
    AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
    uint32_t correctedRate = c.GetCorrectedSourceRate();
    EXPECT_LE(correctedRate, previousCorrected) << "for i=" << i;
    EXPECT_GT(correctedRate, 0u) << "for i=" << i;
    previousCorrected = correctedRate;
  }
  EXPECT_EQ(previousCorrected, 1u);
  AdvanceByOutputDuration(&currentBuffered, &c, oneSec, bufferedLow);
  EXPECT_EQ(c.GetCorrectedSourceRate(), 1u);
}
