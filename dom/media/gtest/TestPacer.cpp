/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <thread>
#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/WaitFor.h"
#include "Pacer.h"
#include "VideoUtils.h"

using namespace mozilla;

template <typename T>
class PacerTest {
 protected:
  explicit PacerTest(TimeDuration aDuplicationInterval)
      : mTaskQueue(TaskQueue::Create(
            GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER), "PacerTest")),
        mPacer(MakeRefPtr<Pacer<T>>(mTaskQueue, aDuplicationInterval)),
        mInterval(aDuplicationInterval) {}

  // Helper for calling `mPacer->Enqueue(...)`. Dispatches an event to the
  // current thread which will enqueue the event to make sure that any listeners
  // registered by a call to `WaitFor(...)` have been registered before events
  // start being processed on a background queue.
  void EnqueueSoon(T aItem, TimeStamp aTime) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "PacerTest::EnqueueSoon",
        [pacer = mPacer, aItem = std::move(aItem), aTime] {
          pacer->Enqueue(std::move(aItem), aTime);
        })));
  }

  void TearDown() {
    mPacer->Shutdown()->Then(mTaskQueue, __func__,
                             [tq = mTaskQueue] { tq->BeginShutdown(); });
  }

  TimeDuration Interval() const { return mInterval; }

  void SetInterval(TimeDuration aInterval) {
    MOZ_ASSERT(NS_IsMainThread());
    mInterval = aInterval;
    mPacer->SetDuplicationInterval(aInterval);
  }

  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<Pacer<T>> mPacer;

 private:
  TimeDuration mInterval;
};

class PacerTestInt : public PacerTest<int>, public ::testing::Test {
 protected:
  explicit PacerTestInt(TimeDuration aDuplicationInterval)
      : PacerTest<int>(aDuplicationInterval) {}

  void TearDown() override { PacerTest::TearDown(); }
};

class PacerTestIntLongDuplication : public PacerTestInt {
 protected:
  PacerTestIntLongDuplication() : PacerTestInt(TimeDuration::FromSeconds(10)) {}
};

class PacerTestIntTenMsDuplication : public PacerTestInt {
 protected:
  PacerTestIntTenMsDuplication()
      : PacerTestInt(TimeDuration::FromMilliseconds(10)) {}
};

class PacerTestIntInfDuplication : public PacerTestInt {
 protected:
  PacerTestIntInfDuplication() : PacerTestInt(TimeDuration::Forever()) {}
};

MATCHER_P(IsDurationPositiveMultipleOf, aDenom,
          std::string(nsPrintfCString("%s a positive non-zero multiple of %s",
                                      negation ? "isn't" : "is",
                                      testing::PrintToString(aDenom).data())
                          .get())) {
  static_assert(std::is_same_v<std::decay_t<decltype(arg)>, TimeDuration>);
  static_assert(std::is_same_v<std::decay_t<decltype(aDenom)>, TimeDuration>);
  const double multiples = arg / aDenom;
  const TimeDuration remainder = arg % aDenom;
  return multiples > 0 && remainder.IsZero();
}

TEST_F(PacerTestIntLongDuplication, Single) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now + d1);

  auto [i, time] = WaitFor(mPacer->PacedItemEvent());
  EXPECT_GE(TimeStamp::Now() - now, d1);
  EXPECT_EQ(i, 1);
  EXPECT_EQ(time - now, d1);
}

TEST_F(PacerTestIntLongDuplication, Past) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now - d1);

  auto [i, time] = WaitFor(mPacer->PacedItemEvent());
  EXPECT_GE(TimeStamp::Now() - now, -d1);
  EXPECT_EQ(i, 1);
  EXPECT_EQ(time - now, -d1);
}

TEST_F(PacerTestIntLongDuplication, TimeReset) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = TimeDuration::FromMilliseconds(200);
  auto d3 = TimeDuration::FromMilliseconds(300);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d3);
  EnqueueSoon(3, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 2)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 3);
    EXPECT_EQ(time - now, d2);
  }
}

TEST_F(PacerTestIntTenMsDuplication, SingleDuplication) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now + d1);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 2)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d1 + Interval());
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1 + Interval());
  }
}

TEST_F(PacerTestIntTenMsDuplication, RacyDuplication1) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = d1 + Interval() - TimeDuration::FromMicroseconds(1);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 3)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2);
  }
  {
    auto [i, time] = items[2];
    EXPECT_GE(TimeStamp::Now() - now, d2 + Interval());
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2 + Interval());
  }
}

TEST_F(PacerTestIntTenMsDuplication, RacyDuplication2) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = d1 + Interval() + TimeDuration::FromMicroseconds(1);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 3)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d1 + Interval());
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1 + Interval());
  }
  {
    auto [i, time] = items[2];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2);
  }
}

TEST_F(PacerTestIntInfDuplication, SetDuplicationInterval) {
  const auto now = TimeStamp::Now();
  const auto t1 = now;
  const auto noDuplication = TimeDuration::FromMilliseconds(250);
  const auto d1 = TimeDuration::FromMilliseconds(33);

  EnqueueSoon(1, t1);
  const auto first = WaitFor(mPacer->PacedItemEvent());
  const auto twoDupes = TakeN(mPacer->PacedItemEvent(), 2);
  while (TimeStamp::Now() < now + noDuplication) {
    if (!NS_ProcessNextEvent(nullptr, /* aMayWait = */ false)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  SetInterval(d1);

  auto items = WaitFor(twoDupes).unwrap();
  const auto t2 =
      std::get<TimeStamp>(items.back()) + TimeDuration::FromMilliseconds(5);
  const auto d2 = TimeDuration::FromMilliseconds(50);
  EnqueueSoon(2, t2);
  SetInterval(d2);
  WaitUntil(mPacer->PacedItemEvent(), [&items](int aItem, TimeStamp aTime) {
    if (aItem == 2) {
      items.push_back({aItem, aTime});
      return true;
    }
    return false;
  });
  const auto last = WaitFor(mPacer->PacedItemEvent());

  items.insert(items.begin(), first);
  items.push_back(last);
  ASSERT_EQ(items.size(), 5U);

  auto [i1, time1] = items[0];
  EXPECT_EQ(i1, 1);
  EXPECT_EQ(time1 - now, t1 - now);

  auto [i2, time2] = items[1];
  EXPECT_EQ(i2, 1);
  EXPECT_GE(time2 - now, noDuplication);

  auto [i3, time3] = items[2];
  EXPECT_EQ(i3, 1);
  EXPECT_THAT(time3 - time2, IsDurationPositiveMultipleOf(d1));

  auto [i4, time4] = items[3];
  EXPECT_EQ(i4, 2);
  EXPECT_EQ(time4 - now, t2 - now);

  auto [i5, time5] = items[4];
  EXPECT_EQ(i5, 2);
  EXPECT_THAT(time5 - time4, IsDurationPositiveMultipleOf(d2));
}
