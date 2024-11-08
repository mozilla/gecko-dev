/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZTestCommon.h"

#include "InputUtils.h"
#include "gtest/gtest.h"

#include "RecentEventsBuffer.h"

struct TestEvent {
 public:
  explicit TestEvent(TimeStamp timeStamp, size_t id);
  TimeStamp mTimeStamp;
  size_t mId;
};

class RecentEventsBufferTest : public ::testing::Test {
 public:
  TimeStamp start;

  void SetUp() { start = TimeStamp::Now(); }
};

TestEvent::TestEvent(TimeStamp timeStamp, size_t id)
    : mTimeStamp(timeStamp), mId(id) {}

TEST_F(RecentEventsBufferTest, Basic) {
  RecentEventsBuffer<TestEvent> buffer(TimeDuration::FromMilliseconds(200));

  // Push three events to the buffer, with the first being the oldest.
  buffer.push(TestEvent(start, 0U));
  buffer.push(TestEvent(start + TimeDuration::FromMilliseconds(100), 1));
  // Push an event to the buffer that will be one millisecond beyond the
  // max age duration from the first event pushed to the buffer.
  buffer.push(TestEvent(start + TimeDuration::FromMilliseconds(201), 2));

  // The oldest timestamp should be dropped when the last event is pushed.
  EXPECT_EQ(buffer.size(), 2U);

  // The first and last events in the buffer are the most recent events
  // that were pushed.
  EXPECT_EQ(buffer.front().mId, 1U);
  EXPECT_EQ(buffer.back().mId, 2U);
}

TEST_F(RecentEventsBufferTest, MinSize) {
  RecentEventsBuffer<TestEvent> buffer(TimeDuration::FromMilliseconds(100), 3);

  // Push two initial events.
  buffer.push(TestEvent(start, 0U));
  buffer.push(TestEvent(start + TimeDuration::FromMilliseconds(1), 1U));

  // Push and event that is greater than the max age from the initial events.
  buffer.push(TestEvent(start + TimeDuration::FromMilliseconds(101), 2U));

  // The minimum size requirement of the buffer should prevent the buffer
  // from removing items.
  EXPECT_EQ(buffer.size(), 3U);

  // Adding one item should allow the initial element to be removed.
  buffer.push(TestEvent(start + TimeDuration::FromMilliseconds(102), 3U));
  EXPECT_EQ(buffer.size(), 3U);
}
