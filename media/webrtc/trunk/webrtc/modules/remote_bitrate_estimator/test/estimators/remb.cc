/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/remb.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/common.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

RembBweSender::RembBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, observer)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetStartBitrate(1000 * kbps);
  bitrate_controller_->SetMinMaxBitrate(1000 * kMinBitrateKbps,
                                        1000 * kMaxBitrateKbps);
}

RembBweSender::~RembBweSender() {
}

void RembBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const RembFeedback& remb_feedback =
      static_cast<const RembFeedback&>(feedback);
  feedback_observer_->OnReceivedEstimatedBitrate(remb_feedback.estimated_bps());
  ReportBlockList report_blocks;
  report_blocks.push_back(remb_feedback.report_block());
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

int64_t RembBweSender::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int RembBweSender::Process() {
  return bitrate_controller_->Process();
}

int RembBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

RembReceiver::RembReceiver(int flow_id, bool plot)
    : BweReceiver(flow_id),
      estimate_log_prefix_(),
      plot_estimate_(plot),
      clock_(0),
      recv_stats_(ReceiveStatistics::Create(&clock_)),
      latest_estimate_bps_(-1),
      estimator_(AbsoluteSendTimeRemoteBitrateEstimatorFactory().Create(
          this,
          &clock_,
          kAimdControl,
          kRemoteBitrateEstimatorMinBitrateBps)) {
  std::stringstream ss;
  ss << "Estimate_" << flow_id_ << "#1";
  estimate_log_prefix_ = ss.str();
  // Default RTT in RemoteRateControl is 200 ms ; 50 ms is more realistic.
  estimator_->OnRttUpdate(50);
}

RembReceiver::~RembReceiver() {
}

void RembReceiver::ReceivePacket(int64_t arrival_time_ms,
                                 const MediaPacket& media_packet) {
  recv_stats_->IncomingPacket(media_packet.header(),
                              media_packet.payload_size(), false);

  latest_estimate_bps_ = -1;

  int64_t step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
  while ((clock_.TimeInMilliseconds() + step_ms) < arrival_time_ms) {
    clock_.AdvanceTimeMilliseconds(step_ms);
    estimator_->Process();
    step_ms = std::max<int64_t>(estimator_->TimeUntilNextProcess(), 0);
  }
  estimator_->IncomingPacket(arrival_time_ms, media_packet.payload_size(),
                             media_packet.header());
  clock_.AdvanceTimeMilliseconds(arrival_time_ms - clock_.TimeInMilliseconds());
  ASSERT_TRUE(arrival_time_ms == clock_.TimeInMilliseconds());
}

FeedbackPacket* RembReceiver::GetFeedback(int64_t now_ms) {
  BWE_TEST_LOGGING_CONTEXT("Remb");
  uint32_t estimated_bps = 0;
  RembFeedback* feedback = NULL;
  if (LatestEstimate(&estimated_bps)) {
    StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
    RTCPReportBlock report_block;
    if (!statisticians.empty()) {
      report_block = BuildReportBlock(statisticians.begin()->second);
    }
    feedback =
        new RembFeedback(flow_id_, now_ms * 1000, estimated_bps, report_block);

    double estimated_kbps = static_cast<double>(estimated_bps) / 1000.0;
    RTC_UNUSED(estimated_kbps);
    if (plot_estimate_) {
      BWE_TEST_LOGGING_PLOT(estimate_log_prefix_, clock_.TimeInMilliseconds(),
                            estimated_kbps);
    }
  }
  return feedback;
}

void RembReceiver::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs,
    unsigned int bitrate) {
}

RTCPReportBlock RembReceiver::BuildReportBlock(
    StreamStatistician* statistician) {
  RTCPReportBlock report_block;
  RtcpStatistics stats;
  if (!statistician->GetStatistics(&stats, true))
    return report_block;
  report_block.fractionLost = stats.fraction_lost;
  report_block.cumulativeLost = stats.cumulative_lost;
  report_block.extendedHighSeqNum = stats.extended_max_sequence_number;
  report_block.jitter = stats.jitter;
  return report_block;
}

bool RembReceiver::LatestEstimate(uint32_t* estimate_bps) {
  if (latest_estimate_bps_ < 0) {
    std::vector<unsigned int> ssrcs;
    unsigned int bps = 0;
    if (!estimator_->LatestEstimate(&ssrcs, &bps)) {
      return false;
    }
    latest_estimate_bps_ = bps;
  }
  *estimate_bps = latest_estimate_bps_;
  return true;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
