/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>

#include "api/stats/rtcstats_objects.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "test/create_frame_generator_capturer.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/peer_scenario/peer_scenario.h"
#include "test/peer_scenario/peer_scenario_client.h"

namespace webrtc {
namespace {

using test::PeerScenario;
using test::PeerScenarioClient;
using ::testing::HasSubstr;

// Helper class used for counting RTCP feedback messages.
class RtcpFeedbackCounter {
 public:
  void Count(const EmulatedIpPacket& packet) {
    if (!IsRtcpPacket(packet.data)) {
      return;
    }
    rtcp::CommonHeader header;
    ASSERT_TRUE(header.Parse(packet.data.cdata(), packet.data.size()));
    if (header.type() != rtcp::Rtpfb::kPacketType) {
      return;
    }
    if (header.fmt() == rtcp::CongestionControlFeedback::kFeedbackMessageType) {
      ++congestion_control_feedback_;
    }
    if (header.fmt() == rtcp::TransportFeedback::kFeedbackMessageType) {
      ++transport_sequence_number_feedback_;
    }
  }

  int FeedbackAccordingToRfc8888() const {
    return congestion_control_feedback_;
  }
  int FeedbackAccordingToTransportCc() const {
    return transport_sequence_number_feedback_;
  }

 private:
  int congestion_control_feedback_ = 0;
  int transport_sequence_number_feedback_ = 0;
};

rtc::scoped_refptr<const RTCStatsReport> GetStatsAndProcess(
    PeerScenario& s,
    PeerScenarioClient* client) {
  auto stats_collector =
      rtc::make_ref_counted<webrtc::MockRTCStatsCollectorCallback>();
  client->pc()->GetStats(stats_collector.get());
  s.ProcessMessages(TimeDelta::Millis(0));
  RTC_CHECK(stats_collector->called());
  return stats_collector->report();
}

DataRate GetAvailableSendBitrate(
    const rtc::scoped_refptr<const RTCStatsReport>& report) {
  auto stats = report->GetStatsOfType<RTCIceCandidatePairStats>();
  if (stats.empty()) {
    return DataRate::Zero();
  }
  return DataRate::BitsPerSec(*stats[0]->available_outgoing_bitrate);
}

TEST(L4STest, NegotiateAndUseCcfbIfEnabled) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  config.disable_encryption = true;
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  // Create network path from caller to callee.
  auto send_node = s.net()->NodeBuilder().Build().node;
  auto ret_node = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {send_node}, callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {ret_node}, caller->endpoint());

  RtcpFeedbackCounter send_node_feedback_counter;
  send_node->router()->SetFilter([&](const EmulatedIpPacket& packet) {
    send_node_feedback_counter.Count(packet);
    return true;
  });
  RtcpFeedbackCounter ret_node_feedback_counter;
  ret_node->router()->SetFilter([&](const EmulatedIpPacket& packet) {
    ret_node_feedback_counter.Count(packet);
    return true;
  });

  auto signaling = s.ConnectSignaling(caller, callee, {send_node}, {ret_node});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;

  caller->CreateVideo("VIDEO_1", video_conf);
  callee->CreateVideo("VIDEO_2", video_conf);

  signaling.StartIceSignaling();

  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp(
      [&](SessionDescriptionInterface* offer) {
        std::string offer_str = absl::StrCat(*offer);
        // Check that the offer contain both congestion control feedback
        // accoring to RFC 8888, and transport-cc and the header extension
        // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
        EXPECT_THAT(offer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
        EXPECT_THAT(offer_str, HasSubstr("transport-cc"));
        EXPECT_THAT(
            offer_str,
            HasSubstr("http://www.ietf.org/id/"
                      "draft-holmer-rmcat-transport-wide-cc-extensions"));
      },
      [&](const SessionDescriptionInterface& answer) {
        std::string answer_str = absl::StrCat(answer);
        EXPECT_THAT(answer_str, HasSubstr("a=rtcp-fb:* ack ccfb\r\n"));
        // Check that the answer does not contain transport-cc nor the
        // header extension
        // http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
        EXPECT_THAT(answer_str, Not(HasSubstr("transport-cc")));
        EXPECT_THAT(
            answer_str,
            Not(HasSubstr(" http://www.ietf.org/id/"
                          "draft-holmer-rmcat-transport-wide-cc-extensions-")));
        offer_exchange_done = true;
      });
  // Wait for SDP negotiation and the packet filter to be setup.
  s.WaitAndProcess(&offer_exchange_done);

  s.ProcessMessages(TimeDelta::Seconds(2));
  EXPECT_GT(send_node_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  // TODO: bugs.webrtc.org/42225697 - Fix bug. Caller sends both transport
  // sequence number feedback and congestion control feedback. So
  // callee still send packets with transport sequence number header extensions
  // even though it has been removed from the answer.
  // EXPECT_EQ(send_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);

  EXPECT_GT(ret_node_feedback_counter.FeedbackAccordingToRfc8888(), 0);
  EXPECT_EQ(ret_node_feedback_counter.FeedbackAccordingToTransportCc(), 0);
}

TEST(L4STest, CallerAdaptToLinkCapacityWithoutEcn) {
  test::ScopedFieldTrials trials(
      "WebRTC-RFC8888CongestionControlFeedback/Enabled/");
  PeerScenario s(*test_info_);

  PeerScenarioClient::Config config = PeerScenarioClient::Config();
  PeerScenarioClient* caller = s.CreateClient(config);
  PeerScenarioClient* callee = s.CreateClient(config);

  auto caller_to_callee = s.net()
                              ->NodeBuilder()
                              .capacity(DataRate::KilobitsPerSec(600))
                              .Build()
                              .node;
  auto callee_to_caller = s.net()->NodeBuilder().Build().node;
  s.net()->CreateRoute(caller->endpoint(), {caller_to_callee},
                       callee->endpoint());
  s.net()->CreateRoute(callee->endpoint(), {callee_to_caller},
                       caller->endpoint());

  auto signaling = s.ConnectSignaling(caller, callee, {caller_to_callee},
                                      {callee_to_caller});
  PeerScenarioClient::VideoSendTrackConfig video_conf;
  video_conf.generator.squares_video->framerate = 15;
  caller->CreateVideo("VIDEO_1", video_conf);

  signaling.StartIceSignaling();
  std::atomic<bool> offer_exchange_done(false);
  signaling.NegotiateSdp([&](const SessionDescriptionInterface& answer) {
    offer_exchange_done = true;
  });
  s.WaitAndProcess(&offer_exchange_done);
  s.ProcessMessages(TimeDelta::Seconds(3));
  DataRate available_bwe =
      GetAvailableSendBitrate(GetStatsAndProcess(s, caller));
  EXPECT_GT(available_bwe.kbps(), 500);
  EXPECT_LT(available_bwe.kbps(), 610);
}

}  // namespace
}  // namespace webrtc
