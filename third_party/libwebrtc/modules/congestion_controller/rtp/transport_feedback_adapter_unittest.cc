/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/rtp/transport_feedback_adapter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "api/transport/network_types.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/sent_packet.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::NotNull;
using ::testing::SizeIs;

constexpr uint32_t kSsrc = 8492;
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
const PacedPacketInfo kPacingInfo2(2, 14, 7000);
const PacedPacketInfo kPacingInfo3(3, 20, 10000);
const PacedPacketInfo kPacingInfo4(4, 22, 10000);

void ComparePacketFeedbackVectors(const std::vector<PacketResult>& truth,
                                  const std::vector<PacketResult>& input) {
  ASSERT_EQ(truth.size(), input.size());
  size_t len = truth.size();
  // truth contains the input data for the test, and input is what will be
  // sent to the bandwidth estimator. truth.arrival_tims_ms is used to
  // populate the transport feedback messages. As these times may be changed
  // (because of resolution limits in the packets, and because of the time
  // base adjustment performed by the TransportFeedbackAdapter at the first
  // packet, the truth[x].arrival_time and input[x].arrival_time may not be
  // equal. However, the difference must be the same for all x.
  TimeDelta arrival_time_delta = truth[0].receive_time - input[0].receive_time;
  for (size_t i = 0; i < len; ++i) {
    RTC_CHECK(truth[i].IsReceived());
    if (input[i].IsReceived()) {
      EXPECT_EQ(truth[i].receive_time - input[i].receive_time,
                arrival_time_delta);
    }
    EXPECT_EQ(truth[i].sent_packet.send_time, input[i].sent_packet.send_time);
    EXPECT_EQ(truth[i].sent_packet.sequence_number,
              input[i].sent_packet.sequence_number);
    EXPECT_EQ(truth[i].sent_packet.size, input[i].sent_packet.size);
    EXPECT_EQ(truth[i].sent_packet.pacing_info,
              input[i].sent_packet.pacing_info);
    EXPECT_EQ(truth[i].sent_packet.audio, input[i].sent_packet.audio);
  }
}

rtcp::TransportFeedback BuildRtcpTransportFeedbackPacket(
    rtc::ArrayView<const PacketResult> packets) {
  rtcp::TransportFeedback feedback;
  feedback.SetBase(packets[0].sent_packet.sequence_number,
                   packets[0].receive_time);

  for (const PacketResult& packet : packets) {
    if (packet.receive_time.IsFinite()) {
      EXPECT_TRUE(feedback.AddReceivedPacket(packet.sent_packet.sequence_number,
                                             packet.receive_time));
    }
  }
  return feedback;
}

PacketResult CreatePacket(int64_t receive_time_ms,
                          int64_t send_time_ms,
                          int64_t sequence_number,
                          size_t payload_size,
                          const PacedPacketInfo& pacing_info) {
  PacketResult res;
  res.receive_time = Timestamp::Millis(receive_time_ms);
  res.sent_packet.send_time = Timestamp::Millis(send_time_ms);
  res.sent_packet.sequence_number = sequence_number;
  res.sent_packet.size = DataSize::Bytes(payload_size);
  res.sent_packet.pacing_info = pacing_info;
  return res;
}

RtpPacketToSend CreatePacketToSend(const PacketResult& packet,
                                   uint32_t ssrc = kSsrc,
                                   uint16_t rtp_sequence_number = 0) {
  RtpPacketToSend send_packet(nullptr);
  send_packet.SetSsrc(ssrc);
  send_packet.SetPayloadSize(packet.sent_packet.size.bytes() -
                             send_packet.headers_size());
  send_packet.SetSequenceNumber(rtp_sequence_number);
  send_packet.set_transport_sequence_number(packet.sent_packet.sequence_number);
  send_packet.set_packet_type(packet.sent_packet.audio
                                  ? RtpPacketMediaType::kAudio
                                  : RtpPacketMediaType::kVideo);

  return send_packet;
}

class MockStreamFeedbackObserver : public webrtc::StreamFeedbackObserver {
 public:
  MOCK_METHOD(void,
              OnPacketFeedbackVector,
              (std::vector<StreamPacketInfo> packet_feedback_vector),
              (override));
};

}  // namespace

class TransportFeedbackAdapterTest : public ::testing::Test {
 public:
  Timestamp TimeNow() const { return Timestamp::Millis(1234); }

  std::optional<TransportPacketsFeedback> CreateAndProcessFeedback(
      rtc::ArrayView<const PacketResult> packets,
      TransportFeedbackAdapter& adapter) {
    rtcp::TransportFeedback rtcp_feedback =
        BuildRtcpTransportFeedbackPacket(packets);
    return adapter.ProcessTransportFeedback(rtcp_feedback, TimeNow());
  }
};

TEST_F(TransportFeedbackAdapterTest, AdaptsFeedbackAndPopulatesSendTimes) {
  TransportFeedbackAdapter adapter;
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(120, 220, 2, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(130, 230, 3, 1500, kPacingInfo1));
  packets.push_back(CreatePacket(140, 240, 4, 1500, kPacingInfo1));

  for (const PacketResult& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet),
                      packet.sent_packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(
        packet.sent_packet.sequence_number, packet.sent_packet.send_time.ms()));
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, FeedbackVectorReportsUnreceived) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketResult> sent_packets = {
      CreatePacket(100, 220, 0, 1500, kPacingInfo0),
      CreatePacket(110, 210, 1, 1500, kPacingInfo0),
      CreatePacket(120, 220, 2, 1500, kPacingInfo0),
      CreatePacket(130, 230, 3, 1500, kPacingInfo0),
      CreatePacket(140, 240, 4, 1500, kPacingInfo0),
      CreatePacket(150, 250, 5, 1500, kPacingInfo0),
      CreatePacket(160, 260, 6, 1500, kPacingInfo0)};
  for (const PacketResult& packet : sent_packets) {
    adapter.AddPacket(CreatePacketToSend(packet),
                      packet.sent_packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(
        packet.sent_packet.sequence_number, packet.sent_packet.send_time.ms()));
  }

  // Note: Important to include the last packet, as only unreceived packets in
  // between received packets can be inferred.
  std::vector<PacketResult> received_packets = {
      sent_packets[0], sent_packets[2], sent_packets[6]};

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(received_packets, adapter);

  ComparePacketFeedbackVectors(sent_packets,
                               adapted_feedback->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, HandlesDroppedPackets) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(100, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo1));
  packets.push_back(CreatePacket(120, 220, 2, 1500, kPacingInfo2));
  packets.push_back(CreatePacket(130, 230, 3, 1500, kPacingInfo3));
  packets.push_back(CreatePacket(140, 240, 4, 1500, kPacingInfo4));

  const uint16_t kSendSideDropBefore = 1;
  const uint16_t kReceiveSideDropAfter = 3;

  std::vector<PacketResult> sent_packets;
  for (const PacketResult& packet : packets) {
    if (packet.sent_packet.sequence_number >= kSendSideDropBefore) {
      sent_packets.push_back(packet);
    }
  }
  for (const PacketResult& packet : sent_packets) {
    adapter.AddPacket(CreatePacketToSend(packet),
                      packet.sent_packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(
        packet.sent_packet.sequence_number, packet.sent_packet.send_time.ms()));
  }

  std::vector<PacketResult> received_packets;
  for (const PacketResult& packet : packets) {
    if (packet.sent_packet.sequence_number <= kReceiveSideDropAfter) {
      received_packets.push_back(packet);
    }
  }

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(received_packets, adapter);

  std::vector<PacketResult> expected_packets(
      packets.begin() + kSendSideDropBefore,
      packets.begin() + kReceiveSideDropAfter + 1);
  // Packets that have timed out on the send-side have lost the
  // information stored on the send-side. And they will not be reported to
  // observers since we won't know that they come from the same networks.
  ComparePacketFeedbackVectors(expected_packets,
                               adapted_feedback->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, FeedbackReportsIfPacketIsAudio) {
  TransportFeedbackAdapter adapter;

  PacketResult packets[] = {CreatePacket(100, 200, 0, 1500, kPacingInfo0)};
  PacketResult& packet = packets[0];
  packet.sent_packet.audio = true;
  adapter.AddPacket(CreatePacketToSend(packet), packet.sent_packet.pacing_info,
                    /*overhead=*/0u, TimeNow());
  adapter.ProcessSentPacket(rtc::SentPacket(packet.sent_packet.sequence_number,
                                            packet.sent_packet.send_time.ms()));

  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ASSERT_THAT(adapted_feedback->packet_feedbacks, SizeIs(1));
  EXPECT_TRUE(adapted_feedback->packet_feedbacks[0].sent_packet.audio);
}

TEST_F(TransportFeedbackAdapterTest, SendTimeWrapsBothWays) {
  TransportFeedbackAdapter adapter;

  TimeDelta kHighArrivalTime =
      rtcp::TransportFeedback::kDeltaTick * (1 << 8) * ((1 << 23) - 1);
  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(kHighArrivalTime.ms() + 64, 210, 0, 1500,
                                 PacedPacketInfo()));
  packets.push_back(CreatePacket(kHighArrivalTime.ms() - 64, 210, 1, 1500,
                                 PacedPacketInfo()));
  packets.push_back(
      CreatePacket(kHighArrivalTime.ms(), 220, 2, 1500, PacedPacketInfo()));

  for (const PacketResult& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet),
                      packet.sent_packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(
        packet.sent_packet.sequence_number, packet.sent_packet.send_time.ms()));
  }

  for (size_t i = 0; i < packets.size(); ++i) {
    std::vector<PacketResult> received_packets = {packets[i]};

    rtcp::TransportFeedback feedback =
        BuildRtcpTransportFeedbackPacket(received_packets);
    rtc::Buffer raw_packet = feedback.Build();
    std::unique_ptr<rtcp::TransportFeedback> parsed_feedback =
        rtcp::TransportFeedback::ParseFrom(raw_packet.data(),
                                           raw_packet.size());
    ASSERT_THAT(parsed_feedback, NotNull());

    std::optional<TransportPacketsFeedback> res =
        adapter.ProcessTransportFeedback(*parsed_feedback, TimeNow());
    ASSERT_TRUE(res.has_value());
    ComparePacketFeedbackVectors(received_packets, res->packet_feedbacks);
  }
}

TEST_F(TransportFeedbackAdapterTest, HandlesArrivalReordering) {
  TransportFeedbackAdapter adapter;

  std::vector<PacketResult> packets;
  packets.push_back(CreatePacket(120, 200, 0, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(110, 210, 1, 1500, kPacingInfo0));
  packets.push_back(CreatePacket(100, 220, 2, 1500, kPacingInfo0));

  for (const PacketResult& packet : packets) {
    adapter.AddPacket(CreatePacketToSend(packet),
                      packet.sent_packet.pacing_info,
                      /*overhead=*/0u, TimeNow());
    adapter.ProcessSentPacket(rtc::SentPacket(
        packet.sent_packet.sequence_number, packet.sent_packet.send_time.ms()));
  }

  // Adapter keeps the packets ordered by sequence number (which is itself
  // assigned by the order of transmission). Reordering by some other criteria,
  // eg. arrival time, is up to the observers.
  std::optional<TransportPacketsFeedback> adapted_feedback =
      CreateAndProcessFeedback(packets, adapter);
  ComparePacketFeedbackVectors(packets, adapted_feedback->packet_feedbacks);
}

TEST_F(TransportFeedbackAdapterTest, IgnoreDuplicatePacketSentCalls) {
  TransportFeedbackAdapter adapter;

  PacketResult packet = CreatePacket(100, 200, 0, 1500, kPacingInfo0);
  RtpPacketToSend packet_to_send =
      CreatePacketToSend(packet, kSsrc, /*rtp_sequence_number=*/0);
  // Add a packet and then mark it as sent.
  adapter.AddPacket(packet_to_send, packet.sent_packet.pacing_info, 0u,
                    TimeNow());
  std::optional<SentPacket> sent_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_TRUE(sent_packet.has_value());

  // Call ProcessSentPacket() again with the same sequence number. This packet
  // has already been marked as sent and the call should be ignored.
  std::optional<SentPacket> duplicate_packet = adapter.ProcessSentPacket(
      rtc::SentPacket(packet.sent_packet.sequence_number,
                      packet.sent_packet.send_time.ms(), rtc::PacketInfo()));
  EXPECT_FALSE(duplicate_packet.has_value());
}

}  // namespace webrtc
