/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RTCPReceiver.
 */
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note: This file has no directory. Lint warning must be ignored.
#include "webrtc/common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

namespace {  // Anonymous namespace; hide utility functions and classes.

// A very simple packet builder class for building RTCP packets.
class PacketBuilder {
 public:
  static const int kMaxPacketSize = 1024;

  struct ReportBlock {
    ReportBlock(uint32_t ssrc, uint32_t extended_max, uint8_t fraction_loss,
                uint32_t cumulative_loss, uint32_t jitter)
        : ssrc(ssrc),
          extended_max(extended_max),
          fraction_loss(fraction_loss),
          cumulative_loss(cumulative_loss),
          jitter(jitter) {}

    uint32_t ssrc;
    uint32_t extended_max;
    uint8_t fraction_loss;
    uint32_t cumulative_loss;
    uint32_t jitter;
  };

  PacketBuilder()
      : pos_(0),
        pos_of_len_(0) {
  }


  void Add8(uint8_t byte) {
    EXPECT_LT(pos_, kMaxPacketSize - 1);
    buffer_[pos_] = byte;
    ++pos_;
  }

  void Add16(uint16_t word) {
    Add8(word >> 8);
    Add8(word & 0xFF);
  }

  void Add32(uint32_t word) {
    Add8(word >> 24);
    Add8((word >> 16) & 0xFF);
    Add8((word >> 8) & 0xFF);
    Add8(word & 0xFF);
  }

  void Add64(uint32_t upper_half, uint32_t lower_half) {
    Add32(upper_half);
    Add32(lower_half);
  }

  // Set the 5-bit value in the 1st byte of the header
  // and the payload type. Set aside room for the length field,
  // and make provision for backpatching it.
  // Note: No way to set the padding bit.
  void AddRtcpHeader(int payload, int format_or_count) {
    PatchLengthField();
    Add8(0x80 | (format_or_count & 0x1F));
    Add8(payload);
    pos_of_len_ = pos_;
    Add16(0xDEAD);  // Initialize length to "clearly illegal".
  }

  void AddTmmbrBandwidth(int mantissa, int exponent, int overhead) {
    // 6 bits exponent, 17 bits mantissa, 9 bits overhead.
    uint32_t word = 0;
    word |= (exponent << 26);
    word |= ((mantissa & 0x1FFFF) << 9);
    word |= (overhead & 0x1FF);
    Add32(word);
  }

  void AddSrPacket(uint32_t sender_ssrc) {
    AddRtcpHeader(200, 0);
    Add32(sender_ssrc);
    Add64(0x10203, 0x4050607);  // NTP timestamp
    Add32(0x10203);  // RTP timestamp
    Add32(0);  // Sender's packet count
    Add32(0);  // Sender's octet count
  }

  void AddRrPacket(uint32_t sender_ssrc, uint32_t rtp_ssrc,
                   uint32_t extended_max, uint8_t fraction_loss,
                   uint32_t cumulative_loss, uint32_t jitter) {
    ReportBlock report_block(rtp_ssrc, extended_max, fraction_loss,
                             cumulative_loss, jitter);
    std::list<ReportBlock> report_block_vector(&report_block,
                                               &report_block + 1);
    AddRrPacketMultipleReportBlocks(sender_ssrc, report_block_vector);
  }

  void AddRrPacketMultipleReportBlocks(
      uint32_t sender_ssrc, const std::list<ReportBlock>& report_blocks) {
    AddRtcpHeader(201, report_blocks.size());
    Add32(sender_ssrc);
    for (std::list<ReportBlock>::const_iterator it = report_blocks.begin();
         it != report_blocks.end(); ++it) {
      AddReportBlock(it->ssrc, it->extended_max, it->fraction_loss,
                     it->cumulative_loss, it->jitter);
    }
  }

  void AddReportBlock(uint32_t rtp_ssrc, uint32_t extended_max,
                      uint8_t fraction_loss, uint32_t cumulative_loss,
                      uint32_t jitter) {
    Add32(rtp_ssrc);
    Add32((fraction_loss << 24) + cumulative_loss);
    Add32(extended_max);
    Add32(jitter);
    Add32(0);  // Last SR.
    Add32(0);  // Delay since last SR.
  }

  void AddXrHeader(uint32_t sender_ssrc) {
    AddRtcpHeader(207, 0);
    Add32(sender_ssrc);
  }

  void AddXrReceiverReferenceTimeBlock(uint32_t ntp_sec, uint32_t ntp_frac) {
    Add8(4);                   // Block type.
    Add8(0);                   // Reserved.
    Add16(2);                  // Length.
    Add64(ntp_sec, ntp_frac);  // NTP timestamp.
  }

  void AddXrDlrrBlock(std::vector<uint32_t>& remote_ssrc) {
    ASSERT_LT(pos_ + 4 + static_cast<int>(remote_ssrc.size())*4,
        kMaxPacketSize-1) << "Max buffer size reached.";
    Add8(5);                      // Block type.
    Add8(0);                      // Reserved.
    Add16(remote_ssrc.size() * 3);  // Length.
    for (size_t i = 0; i < remote_ssrc.size(); ++i) {
      Add32(remote_ssrc.at(i));   // Receiver SSRC.
      Add32(0x10203);             // Last RR.
      Add32(0x40506);             // Delay since last RR.
    }
  }

  void AddXrUnknownBlock() {
    Add8(6);             // Block type.
    Add8(0);             // Reserved.
    Add16(9);            // Length.
    Add32(0);            // Receiver SSRC.
    Add64(0, 0);         // Remaining fields (RFC 3611) are set to zero.
    Add64(0, 0);
    Add64(0, 0);
    Add64(0, 0);
  }

  void AddXrVoipBlock(uint32_t remote_ssrc, uint8_t loss) {
    Add8(7);             // Block type.
    Add8(0);             // Reserved.
    Add16(8);            // Length.
    Add32(remote_ssrc);  // Receiver SSRC.
    Add8(loss);          // Loss rate.
    Add8(0);             // Remaining statistics (RFC 3611) are set to zero.
    Add16(0);
    Add64(0, 0);
    Add64(0, 0);
    Add64(0, 0);
  }

  const uint8_t* packet() {
    PatchLengthField();
    return buffer_;
  }

  unsigned int length() {
    return pos_;
  }
 private:
  void PatchLengthField() {
    if (pos_of_len_ > 0) {
      // Backpatch the packet length. The client must have taken
      // care of proper padding to 32-bit words.
      int this_packet_length = (pos_ - pos_of_len_ - 2);
      ASSERT_EQ(0, this_packet_length % 4)
          << "Packets must be a multiple of 32 bits long"
          << " pos " << pos_ << " pos_of_len " << pos_of_len_;
      buffer_[pos_of_len_] = this_packet_length >> 10;
      buffer_[pos_of_len_+1] = (this_packet_length >> 2) & 0xFF;
      pos_of_len_ = 0;
    }
  }

  int pos_;
  // Where the length field of the current packet is.
  // Note that 0 is not a legal value, so is used for "uninitialized".
  int pos_of_len_;
  uint8_t buffer_[kMaxPacketSize];
};

// This test transport verifies that no functions get called.
class TestTransport : public Transport,
                      public NullRtpData {
 public:
  explicit TestTransport()
      : rtcp_receiver_(NULL) {
  }
  void SetRTCPReceiver(RTCPReceiver* rtcp_receiver) {
    rtcp_receiver_ = rtcp_receiver;
  }
  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    ADD_FAILURE();  // FAIL() gives a compile error.
    return -1;
  }

  // Injects an RTCP packet into the receiver.
  virtual int SendRTCPPacket(int /* ch */, const void *packet, int packet_len) {
    ADD_FAILURE();
    return 0;
  }

  virtual int OnReceivedPayloadData(const uint8_t* payloadData,
                                    const uint16_t payloadSize,
                                    const WebRtcRTPHeader* rtpHeader) {
    ADD_FAILURE();
    return 0;
  }
  RTCPReceiver* rtcp_receiver_;
};

class RtcpReceiverTest : public ::testing::Test {
 protected:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RtcpReceiverTest()
      : over_use_detector_options_(),
        system_clock_(1335900000),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            RemoteBitrateEstimatorFactory().Create(
                &remote_bitrate_observer_,
                &system_clock_,
                kRemoteBitrateEstimatorMinBitrateBps)) {
    test_transport_ = new TestTransport();

    RtpRtcp::Configuration configuration;
    configuration.id = 0;
    configuration.audio = false;
    configuration.clock = &system_clock_;
    configuration.outgoing_transport = test_transport_;
    configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
    rtcp_receiver_ = new RTCPReceiver(0, &system_clock_, rtp_rtcp_impl_);
    test_transport_->SetRTCPReceiver(rtcp_receiver_);
  }
  ~RtcpReceiverTest() {
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
  }

  // Injects an RTCP packet into the receiver.
  // Returns 0 for OK, non-0 for failure.
  int InjectRtcpPacket(const uint8_t* packet,
                       uint16_t packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser(packet,
                                         packet_len,
                                         true);  // Allow non-compound RTCP

    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    EXPECT_EQ(0, rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser));
    rtcp_receiver_->TriggerCallbacksFromRTCPPacket(rtcpPacketInformation);
    // The NACK list is on purpose not copied below as it isn't needed by the
    // test.
    rtcp_packet_info_.rtcpPacketTypeFlags =
        rtcpPacketInformation.rtcpPacketTypeFlags;
    rtcp_packet_info_.remoteSSRC = rtcpPacketInformation.remoteSSRC;
    rtcp_packet_info_.applicationSubType =
        rtcpPacketInformation.applicationSubType;
    rtcp_packet_info_.applicationName = rtcpPacketInformation.applicationName;
    rtcp_packet_info_.report_blocks = rtcpPacketInformation.report_blocks;
    rtcp_packet_info_.rtt = rtcpPacketInformation.rtt;
    rtcp_packet_info_.interArrivalJitter =
        rtcpPacketInformation.interArrivalJitter;
    rtcp_packet_info_.sliPictureId = rtcpPacketInformation.sliPictureId;
    rtcp_packet_info_.rpsiPictureId = rtcpPacketInformation.rpsiPictureId;
    rtcp_packet_info_.receiverEstimatedMaxBitrate =
        rtcpPacketInformation.receiverEstimatedMaxBitrate;
    rtcp_packet_info_.ntp_secs = rtcpPacketInformation.ntp_secs;
    rtcp_packet_info_.ntp_frac = rtcpPacketInformation.ntp_frac;
    rtcp_packet_info_.rtp_timestamp = rtcpPacketInformation.rtp_timestamp;
    rtcp_packet_info_.xr_dlrr_item = rtcpPacketInformation.xr_dlrr_item;
    if (rtcpPacketInformation.VoIPMetric) {
      rtcp_packet_info_.AddVoIPMetric(rtcpPacketInformation.VoIPMetric);
    }
    return 0;
  }

  OverUseDetectorOptions over_use_detector_options_;
  SimulatedClock system_clock_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};


TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const uint8_t bad_packet[] = {0, 0, 0, 0};
  EXPECT_EQ(0, InjectRtcpPacket(bad_packet, sizeof(bad_packet)));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSrPacket) {
  const uint32_t kSenderSsrc = 0x10203;
  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  // The parser will note the remote SSRC on a SR from other than his
  // expected peer, but will not flag that he's gotten a packet.
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(0U,
            kRtcpSr & rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, XrPacketWithZeroReportBlocksIgnored) {
  PacketBuilder p;
  p.AddXrHeader(0x2345);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectXrVoipPacket) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  const uint8_t kLossRate = 123;
  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrVoipBlock(kSourceSsrc, kLossRate);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  ASSERT_TRUE(rtcp_packet_info_.VoIPMetric != NULL);
  EXPECT_EQ(kLossRate, rtcp_packet_info_.VoIPMetric->lossRate);
  EXPECT_EQ(kRtcpXrVoipMetric, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectXrReceiverReferenceTimePacket) {
  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrReceiverReferenceTimeBlock(0x10203, 0x40506);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(kRtcpXrReceiverReferenceTime,
            rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectXrDlrrPacketWithNoSubBlock) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrDlrrBlock(remote_ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_FALSE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, XrDlrrPacketNotToUsIgnored) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc+1);

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrDlrrBlock(remote_ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
  EXPECT_FALSE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrDlrrPacketWithSubBlock) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc);

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrDlrrBlock(remote_ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrDlrrPacketWithMultipleSubBlocks) {
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc+2);
  remote_ssrcs.push_back(kSourceSsrc+1);
  remote_ssrcs.push_back(kSourceSsrc);

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrDlrrBlock(remote_ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrPacketWithMultipleReportBlocks) {
  const uint8_t kLossRate = 123;
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc);

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrDlrrBlock(remote_ssrcs);
  p.AddXrVoipBlock(kSourceSsrc, kLossRate);
  p.AddXrReceiverReferenceTimeBlock(0x10203, 0x40506);

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(static_cast<unsigned int>(kRtcpXrReceiverReferenceTime +
                                      kRtcpXrVoipMetric),
            rtcp_packet_info_.rtcpPacketTypeFlags);
  // The parser should note the DLRR report block item, but not flag the packet
  // since the RTT is not estimated.
  EXPECT_TRUE(rtcp_packet_info_.xr_dlrr_item);
}

TEST_F(RtcpReceiverTest, InjectXrPacketWithUnknownReportBlock) {
  const uint8_t kLossRate = 123;
  const uint32_t kSourceSsrc = 0x123456;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);
  std::vector<uint32_t> remote_ssrcs;
  remote_ssrcs.push_back(kSourceSsrc);

  PacketBuilder p;
  p.AddXrHeader(0x2345);
  p.AddXrVoipBlock(kSourceSsrc, kLossRate);
  p.AddXrUnknownBlock();
  p.AddXrReceiverReferenceTimeBlock(0x10203, 0x40506);

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(static_cast<unsigned int>(kRtcpXrReceiverReferenceTime +
                                      kRtcpXrVoipMetric),
            rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST(RtcpUtilityTest, MidNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, RTCPUtility::MidNtp(kNtpSec, kNtpFrac));
}

TEST_F(RtcpReceiverTest, TestXrRrRttInitiallyFalse) {
  uint16_t rtt_ms;
  EXPECT_FALSE(rtcp_receiver_->GetAndResetXrRrRtt(&rtt_ms));
}

TEST_F(RtcpReceiverTest, LastReceivedXrReferenceTimeInfoInitiallyFalse) {
  RtcpReceiveTimeInfo info;
  EXPECT_FALSE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
}

TEST_F(RtcpReceiverTest, GetLastReceivedXrReferenceTimeInfo) {
  const uint32_t kSenderSsrc = 0x123456;
  const uint32_t kNtpSec = 0x10203;
  const uint32_t kNtpFrac = 0x40506;
  const uint32_t kNtpMid = RTCPUtility::MidNtp(kNtpSec, kNtpFrac);

  PacketBuilder p;
  p.AddXrHeader(kSenderSsrc);
  p.AddXrReceiverReferenceTimeBlock(kNtpSec, kNtpFrac);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(kRtcpXrReceiverReferenceTime,
      rtcp_packet_info_.rtcpPacketTypeFlags);

  RtcpReceiveTimeInfo info;
  EXPECT_TRUE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
  EXPECT_EQ(kSenderSsrc, info.sourceSSRC);
  EXPECT_EQ(kNtpMid, info.lastRR);
  EXPECT_EQ(0U, info.delaySinceLastRR);

  system_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_TRUE(rtcp_receiver_->LastReceivedXrReferenceTimeInfo(&info));
  EXPECT_EQ(65536U, info.delaySinceLastRR);
}

TEST_F(RtcpReceiverTest, ReceiveReportTimeout) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x40506;
  const int64_t kRtcpIntervalMs = 1000;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  uint32_t sequence_number = 1234;
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);

  // No RR received, shouldn't trigger a timeout.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR and advance the clock just enough to not trigger a timeout.
  PacketBuilder p1;
  p1.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number, 0, 0, 0);
  EXPECT_EQ(0, InjectRtcpPacket(p1.packet(), p1.length()));
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs - 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR with the same extended max as the previous RR to trigger a
  // sequence number timeout, but not a RR timeout.
  PacketBuilder p2;
  p2.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number, 0, 0, 0);
  EXPECT_EQ(0, InjectRtcpPacket(p2.packet(), p2.length()));
  system_clock_.AdvanceTimeMilliseconds(2);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Advance clock enough to trigger an RR timeout too.
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));

  // We should only get one timeout even though we still haven't received a new
  // RR.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a new RR with increase sequence number to reset timers.
  PacketBuilder p3;
  sequence_number++;
  p2.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number, 0, 0, 0);
  EXPECT_EQ(0, InjectRtcpPacket(p2.packet(), p2.length()));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Verify we can get a timeout again once we've received new RR.
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  PacketBuilder p4;
  p4.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number, 0, 0, 0);
  EXPECT_EQ(0, InjectRtcpPacket(p4.packet(), p4.length()));
  system_clock_.AdvanceTimeMilliseconds(kRtcpIntervalMs + 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
}

TEST_F(RtcpReceiverTest, TmmbrReceivedWithNoIncomingPacket) {
  // This call is expected to fail because no data has arrived.
  EXPECT_EQ(-1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TwoReportBlocks) {
  const uint32_t kSenderSsrc = 0x10203;
  const int kNumSsrcs = 2;
  const uint32_t kSourceSsrcs[kNumSsrcs] = {0x40506, 0x50607};
  uint32_t sequence_numbers[kNumSsrcs] = {10, 12423};

  std::set<uint32_t> ssrcs(kSourceSsrcs, kSourceSsrcs + kNumSsrcs);
  rtcp_receiver_->SetSsrcs(kSourceSsrcs[0], ssrcs);

  PacketBuilder packet;
  std::list<PacketBuilder::ReportBlock> report_blocks;
  report_blocks.push_back(PacketBuilder::ReportBlock(
      kSourceSsrcs[0], sequence_numbers[0], 10, 5, 0));
  report_blocks.push_back(PacketBuilder::ReportBlock(
      kSourceSsrcs[1], sequence_numbers[1], 0, 0, 0));
  packet.AddRrPacketMultipleReportBlocks(kSenderSsrc, report_blocks);
  EXPECT_EQ(0, InjectRtcpPacket(packet.packet(), packet.length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(10, rtcp_packet_info_.report_blocks.front().fractionLost);
  EXPECT_EQ(0, rtcp_packet_info_.report_blocks.back().fractionLost);

  PacketBuilder packet2;
  report_blocks.clear();
  report_blocks.push_back(PacketBuilder::ReportBlock(
      kSourceSsrcs[0], sequence_numbers[0], 0, 0, 0));
  report_blocks.push_back(PacketBuilder::ReportBlock(
      kSourceSsrcs[1], sequence_numbers[1], 20, 10, 0));
  packet2.AddRrPacketMultipleReportBlocks(kSenderSsrc, report_blocks);
  EXPECT_EQ(0, InjectRtcpPacket(packet2.packet(), packet2.length()));
  ASSERT_EQ(2u, rtcp_packet_info_.report_blocks.size());
  EXPECT_EQ(0, rtcp_packet_info_.report_blocks.front().fractionLost);
  EXPECT_EQ(20, rtcp_packet_info_.report_blocks.back().fractionLost);
}

TEST_F(RtcpReceiverTest, TmmbrPacketAccepted) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kMediaFlowSsrc);
  p.AddTmmbrBandwidth(30000, 0, 0);  // 30 Kbits/sec bandwidth, no overhead.

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(1);
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(1, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  EXPECT_EQ(kMediaRecipientSsrc, candidate_set.Ssrc(0));
}

TEST_F(RtcpReceiverTest, TmmbrPacketNotForUsIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  const uint32_t kOtherMediaFlowSsrc = 0x9999;

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kOtherMediaFlowSsrc);  // This SSRC is not what we're sending.
  p.AddTmmbrBandwidth(30000, 0, 0);

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrPacketZeroRateIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kMediaFlowSsrc);
  p.AddTmmbrBandwidth(0, 0, 0);  // Rate zero.

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrThreeConstraintsTimeOut) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  std::set<uint32_t> ssrcs;
  ssrcs.insert(kMediaFlowSsrc);  // Matches "media source" above.
  rtcp_receiver_->SetSsrcs(kMediaFlowSsrc, ssrcs);

  // Inject 3 packets "from" kMediaRecipientSsrc, Ssrc+1, Ssrc+2.
  // The times of arrival are starttime + 0, starttime + 5 and starttime + 10.
  for (uint32_t ssrc = kMediaRecipientSsrc;
       ssrc < kMediaRecipientSsrc+3; ++ssrc) {
    PacketBuilder p;
    p.AddSrPacket(kSenderSsrc);
    // TMMBR packet.
    p.AddRtcpHeader(205, 3);
    p.Add32(kSenderSsrc);
    p.Add32(ssrc);
    p.Add32(kMediaFlowSsrc);
    p.AddTmmbrBandwidth(30000, 0, 0);  // 30 Kbits/sec bandwidth, no overhead.

    EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
    // 5 seconds between each packet.
    system_clock_.AdvanceTimeMilliseconds(5000);
  }
  // It is now starttime+15.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(3);
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  // We expect the timeout to be 25 seconds. Advance the clock by 12
  // seconds, timing out the first packet.
  system_clock_.AdvanceTimeMilliseconds(12000);
  // Odd behaviour: Just counting them does not trigger the timeout.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  // Odd behaviour: There's only one left after timeout, not 2.
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_EQ(kMediaRecipientSsrc + 2, candidate_set.Ssrc(0));
}

TEST_F(RtcpReceiverTest, Callbacks) {
  class RtcpCallbackImpl : public RtcpStatisticsCallback {
   public:
    RtcpCallbackImpl() : RtcpStatisticsCallback(), ssrc_(0) {}
    virtual ~RtcpCallbackImpl() {}

    virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                   uint32_t ssrc) {
      stats_ = statistics;
      ssrc_ = ssrc;
    }

    bool Matches(uint32_t ssrc, uint32_t extended_max, uint8_t fraction_loss,
                 uint32_t cumulative_loss, uint32_t jitter) {
      return ssrc_ == ssrc &&
          stats_.fraction_lost == fraction_loss &&
          stats_.cumulative_lost == cumulative_loss &&
          stats_.extended_max_sequence_number == extended_max &&
          stats_.jitter == jitter;
    }

    RtcpStatistics stats_;
    uint32_t ssrc_;
  } callback;

  rtcp_receiver_->RegisterRtcpStatisticsCallback(&callback);

  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x123456;
  const uint8_t fraction_loss = 3;
  const uint32_t cumulative_loss = 7;
  const uint32_t jitter = 9;
  uint32_t sequence_number = 1234;

  std::set<uint32_t> ssrcs;
  ssrcs.insert(kSourceSsrc);
  rtcp_receiver_->SetSsrcs(kSourceSsrc, ssrcs);

  // First packet, all numbers should just propagate
  PacketBuilder p1;
  p1.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number,
                 fraction_loss, cumulative_loss, jitter);
  EXPECT_EQ(0, InjectRtcpPacket(p1.packet(), p1.length()));
  EXPECT_TRUE(callback.Matches(kSourceSsrc, sequence_number, fraction_loss,
                               cumulative_loss, jitter));

  rtcp_receiver_->RegisterRtcpStatisticsCallback(NULL);

  // Add arbitrary numbers, callback should not be called (retain old values)
  PacketBuilder p2;
  p2.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number + 1, 42, 137, 4711);
  EXPECT_EQ(0, InjectRtcpPacket(p2.packet(), p2.length()));
  EXPECT_TRUE(callback.Matches(kSourceSsrc, sequence_number, fraction_loss,
                               cumulative_loss, jitter));
}

}  // Anonymous namespace

}  // namespace webrtc
