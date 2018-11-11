/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/call_stats.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

class MockStatsObserver : public CallStatsObserver {
 public:
  MockStatsObserver() {}
  virtual ~MockStatsObserver() {}

  MOCK_METHOD1(OnRttUpdate, void(int64_t));
};

class CallStatsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    TickTime::UseFakeClock(12345);
    call_stats_.reset(new CallStats());
  }
  rtc::scoped_ptr<CallStats> call_stats_;
};

TEST_F(CallStatsTest, AddAndTriggerCallback) {
  MockStatsObserver stats_observer;
  RtcpRttStats* rtcp_rtt_stats = call_stats_->rtcp_rtt_stats();
  call_stats_->RegisterStatsObserver(&stats_observer);
  TickTime::AdvanceFakeClock(1000);
  EXPECT_EQ(0, rtcp_rtt_stats->LastProcessedRtt());

  const int64_t kRtt = 25;
  rtcp_rtt_stats->OnRttUpdate(kRtt);
  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt))
      .Times(1);
  call_stats_->Process();
  EXPECT_EQ(kRtt, rtcp_rtt_stats->LastProcessedRtt());

  const int64_t kRttTimeOutMs = 1500 + 10;
  TickTime::AdvanceFakeClock(kRttTimeOutMs);
  EXPECT_CALL(stats_observer, OnRttUpdate(_))
      .Times(0);
  call_stats_->Process();
  EXPECT_EQ(0, rtcp_rtt_stats->LastProcessedRtt());

  call_stats_->DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStatsTest, ProcessTime) {
  MockStatsObserver stats_observer;
  call_stats_->RegisterStatsObserver(&stats_observer);
  RtcpRttStats* rtcp_rtt_stats = call_stats_->rtcp_rtt_stats();
  rtcp_rtt_stats->OnRttUpdate(100);

  // Time isn't updated yet.
  EXPECT_CALL(stats_observer, OnRttUpdate(_))
      .Times(0);
  call_stats_->Process();

  // Advance clock and verify we get an update.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(stats_observer, OnRttUpdate(_))
      .Times(1);
  call_stats_->Process();

  // Advance clock just too little to get an update.
  TickTime::AdvanceFakeClock(999);
  rtcp_rtt_stats->OnRttUpdate(100);
  EXPECT_CALL(stats_observer, OnRttUpdate(_))
      .Times(0);
  call_stats_->Process();

  // Advance enough to trigger a new update.
  TickTime::AdvanceFakeClock(1);
  EXPECT_CALL(stats_observer, OnRttUpdate(_))
      .Times(1);
  call_stats_->Process();

  call_stats_->DeregisterStatsObserver(&stats_observer);
}

// Verify all observers get correct estimates and observers can be added and
// removed.
TEST_F(CallStatsTest, MultipleObservers) {
  MockStatsObserver stats_observer_1;
  call_stats_->RegisterStatsObserver(&stats_observer_1);
  // Add the second observer twice, there should still be only one report to the
  // observer.
  MockStatsObserver stats_observer_2;
  call_stats_->RegisterStatsObserver(&stats_observer_2);
  call_stats_->RegisterStatsObserver(&stats_observer_2);

  RtcpRttStats* rtcp_rtt_stats = call_stats_->rtcp_rtt_stats();
  const int64_t kRtt = 100;
  rtcp_rtt_stats->OnRttUpdate(kRtt);

  // Verify both observers are updated.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt))
      .Times(1);
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt))
      .Times(1);
  call_stats_->Process();

  // Deregister the second observer and verify update is only sent to the first
  // observer.
  call_stats_->DeregisterStatsObserver(&stats_observer_2);
  rtcp_rtt_stats->OnRttUpdate(kRtt);
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt))
      .Times(1);
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt))
      .Times(0);
  call_stats_->Process();

  // Deregister the first observer.
  call_stats_->DeregisterStatsObserver(&stats_observer_1);
  rtcp_rtt_stats->OnRttUpdate(kRtt);
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt))
      .Times(0);
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt))
      .Times(0);
  call_stats_->Process();
}

// Verify increasing and decreasing rtt triggers callbacks with correct values.
TEST_F(CallStatsTest, ChangeRtt) {
  MockStatsObserver stats_observer;
  call_stats_->RegisterStatsObserver(&stats_observer);
  RtcpRttStats* rtcp_rtt_stats = call_stats_->rtcp_rtt_stats();

  // Advance clock to be ready for an update.
  TickTime::AdvanceFakeClock(1000);

  // Set a first value and verify the callback is triggered.
  const int64_t kFirstRtt = 100;
  rtcp_rtt_stats->OnRttUpdate(kFirstRtt);
  EXPECT_CALL(stats_observer, OnRttUpdate(kFirstRtt))
      .Times(1);
  call_stats_->Process();

  // Increase rtt and verify the new value is reported.
  TickTime::AdvanceFakeClock(1000);
  const int64_t kHighRtt = kFirstRtt + 20;
  rtcp_rtt_stats->OnRttUpdate(kHighRtt);
  EXPECT_CALL(stats_observer, OnRttUpdate(kHighRtt))
      .Times(1);
  call_stats_->Process();

  // Increase time enough for a new update, but not too much to make the
  // rtt invalid. Report a lower rtt and verify the old/high value still is sent
  // in the callback.
  TickTime::AdvanceFakeClock(1000);
  const int64_t kLowRtt = kFirstRtt - 20;
  rtcp_rtt_stats->OnRttUpdate(kLowRtt);
  EXPECT_CALL(stats_observer, OnRttUpdate(kHighRtt))
      .Times(1);
  call_stats_->Process();

  // Advance time to make the high report invalid, the lower rtt should now be
  // in the callback.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(stats_observer, OnRttUpdate(kLowRtt))
      .Times(1);
  call_stats_->Process();

  call_stats_->DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStatsTest, LastProcessedRtt) {
  MockStatsObserver stats_observer;
  call_stats_->RegisterStatsObserver(&stats_observer);
  RtcpRttStats* rtcp_rtt_stats = call_stats_->rtcp_rtt_stats();
  TickTime::AdvanceFakeClock(1000);

  // Set a first values and verify that LastProcessedRtt initially returns the
  // average rtt.
  const int64_t kRttLow = 10;
  const int64_t kRttHigh = 30;
  const int64_t kAvgRtt = 20;
  rtcp_rtt_stats->OnRttUpdate(kRttLow);
  rtcp_rtt_stats->OnRttUpdate(kRttHigh);
  EXPECT_CALL(stats_observer, OnRttUpdate(kRttHigh))
      .Times(1);
  call_stats_->Process();
  EXPECT_EQ(kAvgRtt, rtcp_rtt_stats->LastProcessedRtt());

  // Update values and verify LastProcessedRtt.
  TickTime::AdvanceFakeClock(1000);
  rtcp_rtt_stats->OnRttUpdate(kRttLow);
  rtcp_rtt_stats->OnRttUpdate(kRttHigh);
  EXPECT_CALL(stats_observer, OnRttUpdate(kRttHigh))
      .Times(1);
  call_stats_->Process();
  EXPECT_EQ(kAvgRtt, rtcp_rtt_stats->LastProcessedRtt());

  call_stats_->DeregisterStatsObserver(&stats_observer);
}

}  // namespace webrtc
