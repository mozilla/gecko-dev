/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/codecs/interface/mock/mock_video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/mock/mock_vcm_callbacks.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using ::testing::_;
using ::testing::NiceMock;

namespace webrtc {
namespace vcm {
namespace {

class TestVideoReceiver : public ::testing::Test {
 protected:
  static const int kUnusedPayloadType = 10;

  TestVideoReceiver() : clock_(0) {}

  virtual void SetUp() {
    receiver_.reset(new VideoReceiver(&clock_, &event_factory_));
    EXPECT_EQ(0, receiver_->InitializeReceiver());
    EXPECT_EQ(0,
              receiver_->RegisterExternalDecoder(
                  &decoder_, kUnusedPayloadType, true));
    const size_t kMaxNackListSize = 250;
    const int kMaxPacketAgeToNack = 450;
    receiver_->SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack, 0);

    memset(&settings_, 0, sizeof(settings_));
    EXPECT_EQ(0, VideoCodingModule::Codec(kVideoCodecVP8, &settings_));
    settings_.plType = kUnusedPayloadType;  // Use the mocked encoder.
    EXPECT_EQ(0, receiver_->RegisterReceiveCodec(&settings_, 1, true));
  }

  void InsertAndVerifyPaddingFrame(const uint8_t* payload,
                                   int length,
                                   WebRtcRTPHeader* header) {
    ASSERT_TRUE(header != NULL);
    for (int j = 0; j < 5; ++j) {
      // Padding only packets are passed to the VCM with payload size 0.
      EXPECT_EQ(0, receiver_->IncomingPacket(payload, 0, *header));
      ++header->header.sequenceNumber;
    }
    EXPECT_EQ(0, receiver_->Process());
    EXPECT_CALL(decoder_, Decode(_, _, _, _, _)).Times(0);
    EXPECT_EQ(VCM_FRAME_NOT_READY, receiver_->Decode(0));
  }

  void InsertAndVerifyDecodableFrame(const uint8_t* payload,
                                     int length,
                                     WebRtcRTPHeader* header) {
    ASSERT_TRUE(header != NULL);
    EXPECT_EQ(0, receiver_->IncomingPacket(payload, length, *header));
    ++header->header.sequenceNumber;
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);
    EXPECT_EQ(0, receiver_->Process());
    EXPECT_CALL(decoder_, Decode(_, _, _, _, _)).Times(1);
    EXPECT_EQ(0, receiver_->Decode(0));
  }

  SimulatedClock clock_;
  NullEventFactory event_factory_;
  VideoCodec settings_;
  NiceMock<MockVideoDecoder> decoder_;
  NiceMock<MockPacketRequestCallback> packet_request_callback_;

  scoped_ptr<VideoReceiver> receiver_;
};

TEST_F(TestVideoReceiver, PaddingOnlyFrames) {
  EXPECT_EQ(0, receiver_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(
      0, receiver_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kPaddingSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRtpVideoVp8;
  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);
    InsertAndVerifyPaddingFrame(payload, 0, &header);
    clock_.AdvanceTimeMilliseconds(33);
    header.header.timestamp += 3000;
  }
}

TEST_F(TestVideoReceiver, PaddingOnlyFramesWithLosses) {
  EXPECT_EQ(0, receiver_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(
      0, receiver_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kFrameSize = 1200;
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kFrameSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRtpVideoVp8;
  // Insert one video frame to get one frame decoded.
  header.frameType = kVideoFrameKey;
  header.type.Video.isFirstPacket = true;
  header.header.markerBit = true;
  InsertAndVerifyDecodableFrame(payload, kFrameSize, &header);
  clock_.AdvanceTimeMilliseconds(33);
  header.header.timestamp += 3000;

  header.frameType = kFrameEmpty;
  header.type.Video.isFirstPacket = false;
  header.header.markerBit = false;
  // Insert padding frames.
  for (int i = 0; i < 10; ++i) {
    // Lose one packet from the 6th frame.
    if (i == 5) {
      ++header.header.sequenceNumber;
    }
    // Lose the 4th frame.
    if (i == 3) {
      header.header.sequenceNumber += 5;
    } else {
      if (i > 3 && i < 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 5)).Times(1);
      } else if (i >= 5) {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, 6)).Times(1);
      } else {
        EXPECT_CALL(packet_request_callback_, ResendPackets(_, _)).Times(0);
      }
      InsertAndVerifyPaddingFrame(payload, 0, &header);
    }
    clock_.AdvanceTimeMilliseconds(33);
    header.header.timestamp += 3000;
  }
}

TEST_F(TestVideoReceiver, PaddingOnlyAndVideo) {
  EXPECT_EQ(0, receiver_->SetVideoProtection(kProtectionNack, true));
  EXPECT_EQ(
      0, receiver_->RegisterPacketRequestCallback(&packet_request_callback_));
  const unsigned int kFrameSize = 1200;
  const unsigned int kPaddingSize = 220;
  const uint8_t payload[kFrameSize] = {0};
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  header.frameType = kFrameEmpty;
  header.type.Video.isFirstPacket = false;
  header.header.markerBit = false;
  header.header.paddingLength = kPaddingSize;
  header.header.payloadType = kUnusedPayloadType;
  header.header.ssrc = 1;
  header.header.headerLength = 12;
  header.type.Video.codec = kRtpVideoVp8;
  header.type.Video.codecHeader.VP8.pictureId = -1;
  header.type.Video.codecHeader.VP8.tl0PicIdx = -1;
  for (int i = 0; i < 3; ++i) {
    // Insert 2 video frames.
    for (int j = 0; j < 2; ++j) {
      if (i == 0 && j == 0)  // First frame should be a key frame.
        header.frameType = kVideoFrameKey;
      else
        header.frameType = kVideoFrameDelta;
      header.type.Video.isFirstPacket = true;
      header.header.markerBit = true;
      InsertAndVerifyDecodableFrame(payload, kFrameSize, &header);
      clock_.AdvanceTimeMilliseconds(33);
      header.header.timestamp += 3000;
    }

    // Insert 2 padding only frames.
    header.frameType = kFrameEmpty;
    header.type.Video.isFirstPacket = false;
    header.header.markerBit = false;
    for (int j = 0; j < 2; ++j) {
      // InsertAndVerifyPaddingFrame(payload, 0, &header);
      clock_.AdvanceTimeMilliseconds(33);
      header.header.timestamp += 3000;
    }
  }
}

TEST_F(TestVideoReceiver, ReceiverDelay) {
  EXPECT_EQ(0, receiver_->SetMinReceiverDelay(0));
  EXPECT_EQ(0, receiver_->SetMinReceiverDelay(5000));
  EXPECT_EQ(-1, receiver_->SetMinReceiverDelay(-100));
  EXPECT_EQ(-1, receiver_->SetMinReceiverDelay(10010));
}

}  // namespace
}  // namespace vcm
}  // namespace webrtc
