/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/frame_instrumentation_generator.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/types/variant.h"
#include "api/scoped_refptr.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "common_video/frame_instrumentation_data.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kDefaultScaledWidth = 4;
constexpr int kDefaultScaledHeight = 4;

scoped_refptr<I420Buffer> MakeDefaultI420FrameBuffer() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  const int kDefaultPixelValue = 30;
  std::vector<uint8_t> kDefaultYContent(16, kDefaultPixelValue);
  std::vector<uint8_t> kDefaultUContent(4, kDefaultPixelValue);
  std::vector<uint8_t> kDefaultVContent(4, kDefaultPixelValue);

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent.data(), kDefaultLumaWidth,
                          kDefaultUContent.data(), kDefaultChromaWidth,
                          kDefaultVContent.data(), kDefaultChromaWidth);
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNoFramesHaveBeenProvided) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecGeneric);

  EXPECT_FALSE(generator.OnEncodedImage(EncodedImage()).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNoFrameWithTheSameTimestampIsProvided) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecGeneric);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(2);

  generator.OnCapturedFrame(frame);

  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenTheFirstFrameOfASpatialOrSimulcastLayerIsNotAKeyFrame) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecGeneric);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // Delta frame with no preceding key frame.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.SetFrameType(VideoFrameType::kVideoFrameDelta);
  encoded_image.SetSpatialIndex(0);
  encoded_image.SetSimulcastIndex(0);

  generator.OnCapturedFrame(frame);

  // The first frame of a spatial or simulcast layer is not a key frame.
  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenQpIsUnsetAndNotParseable) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecGeneric);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // Frame where QP is unset and QP is not parseable from the encoded data.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.SetFrameType(VideoFrameType::kVideoFrameKey);

  generator.OnCapturedFrame(frame);

  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

#if GTEST_HAS_DEATH_TEST
TEST(FrameInstrumentationGeneratorTest, FailsWhenCodecIsUnsupported) {
  // No available mapping from codec to filter parameters.
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecGeneric);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image.qp_ = 10;

  generator.OnCapturedFrame(frame);

  EXPECT_DEATH(generator.OnEncodedImage(encoded_image),
               "Codec type Generic is not supported");
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForVP8KeyFrameWithQpSet) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP8);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  // VP8 key frame with QP set.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image.qp_ = 10;
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  std::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
      data = generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(data.has_value());
  ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data));
  FrameInstrumentationData frame_instrumentation_data =
      absl::get<FrameInstrumentationData>(*data);
  EXPECT_EQ(frame_instrumentation_data.sequence_index, 0);
  EXPECT_TRUE(frame_instrumentation_data.communicate_upper_bits);
  EXPECT_NE(frame_instrumentation_data.std_dev, 0.0);
  EXPECT_NE(frame_instrumentation_data.luma_error_threshold, 0);
  EXPECT_NE(frame_instrumentation_data.chroma_error_threshold, 0);
  EXPECT_FALSE(frame_instrumentation_data.sample_values.empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataWhenQpIsParseable) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP8);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // VP8 key frame with parseable QP.
  constexpr uint8_t kCodedFrameVp8Qp25[] = {
      0x10, 0x02, 0x00, 0x9d, 0x01, 0x2a, 0x10, 0x00, 0x10, 0x00,
      0x02, 0x47, 0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x0c,
      0x82, 0x00, 0x0c, 0x0d, 0x60, 0x00, 0xfe, 0xfc, 0x5c, 0xd0};
  scoped_refptr<EncodedImageBuffer> encoded_image_buffer =
      EncodedImageBuffer::Create(kCodedFrameVp8Qp25,
                                 sizeof(kCodedFrameVp8Qp25));
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image.SetEncodedData(encoded_image_buffer);
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  std::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
      data = generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(data.has_value());
  ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data));
  FrameInstrumentationData frame_instrumentation_data =
      absl::get<FrameInstrumentationData>(*data);
  EXPECT_EQ(frame_instrumentation_data.sequence_index, 0);
  EXPECT_TRUE(frame_instrumentation_data.communicate_upper_bits);
  EXPECT_NE(frame_instrumentation_data.std_dev, 0.0);
  EXPECT_NE(frame_instrumentation_data.luma_error_threshold, 0);
  EXPECT_NE(frame_instrumentation_data.chroma_error_threshold, 0);
  EXPECT_FALSE(frame_instrumentation_data.sample_values.empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForUpperLayerOfAnSvcKeyFrame) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP9);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image1.SetSpatialIndex(0);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is an upper layer of an SVC key frame.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(1);
  encoded_image2.SetFrameType(VideoFrameType::kVideoFrameDelta);
  encoded_image2.SetSpatialIndex(1);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  generator.OnEncodedImage(encoded_image1);
  std::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
      data = generator.OnEncodedImage(encoded_image2);

  ASSERT_TRUE(data.has_value());
  ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data));
  FrameInstrumentationData frame_instrumentation_data =
      absl::get<FrameInstrumentationData>(*data);
  EXPECT_EQ(frame_instrumentation_data.sequence_index, 0);
  EXPECT_TRUE(frame_instrumentation_data.communicate_upper_bits);
  EXPECT_NE(frame_instrumentation_data.std_dev, 0.0);
  EXPECT_NE(frame_instrumentation_data.luma_error_threshold, 0);
  EXPECT_NE(frame_instrumentation_data.chroma_error_threshold, 0);
  EXPECT_FALSE(frame_instrumentation_data.sample_values.empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNotEnoughTimeHasPassedSinceLastSampledFrame) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP8);
  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(1)
                          .build();
  VideoFrame frame2 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(2)
                          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image1.SetSpatialIndex(0);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is too recent in comparison to the last sampled frame:
  // passed time < 90'000.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.SetFrameType(VideoFrameType::kVideoFrameDelta);
  encoded_image2.SetSpatialIndex(0);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame1);
  generator.OnCapturedFrame(frame2);
  generator.OnEncodedImage(encoded_image1);

  ASSERT_FALSE(generator.OnEncodedImage(encoded_image2).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForUpperLayerOfASecondSvcKeyFrame) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP9);
  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(1)
                          .build();
  VideoFrame frame2 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(2)
                          .build();
  for (const VideoFrame& frame : {frame1, frame2}) {
    EncodedImage encoded_image1;
    encoded_image1.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image1.SetFrameType(VideoFrameType::kVideoFrameKey);
    encoded_image1.SetSpatialIndex(0);
    encoded_image1.qp_ = 10;
    encoded_image1._encodedWidth = kDefaultScaledWidth;
    encoded_image1._encodedHeight = kDefaultScaledHeight;

    EncodedImage encoded_image2;
    encoded_image2.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image2.SetFrameType(VideoFrameType::kVideoFrameDelta);
    encoded_image2.SetSpatialIndex(1);
    encoded_image2.qp_ = 10;
    encoded_image2._encodedWidth = kDefaultScaledWidth;
    encoded_image2._encodedHeight = kDefaultScaledHeight;

    generator.OnCapturedFrame(frame1);
    generator.OnCapturedFrame(frame2);

    std::optional<
        absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
        data1 = generator.OnEncodedImage(encoded_image1);

    std::optional<
        absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
        data2 = generator.OnEncodedImage(encoded_image2);

    ASSERT_TRUE(data1.has_value());
    ASSERT_TRUE(data2.has_value());
    ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data1));

    ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data2));

    EXPECT_TRUE(
        absl::get<FrameInstrumentationData>(*data1).communicate_upper_bits);
    EXPECT_TRUE(
        absl::get<FrameInstrumentationData>(*data2).communicate_upper_bits);
  }
}

TEST(FrameInstrumentationGeneratorTest,
     OutputsDeltaFrameInstrumentationDataForSimulcast) {
  FrameInstrumentationGenerator generator(VideoCodecType::kVideoCodecVP9);
  bool has_found_delta_frame = false;
  // 34 frames is the minimum number of frames to be able to sample a delta
  // frame.
  for (int i = 0; i < 34; ++i) {
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                           .set_rtp_timestamp(i)
                           .build();
    EncodedImage encoded_image1;
    encoded_image1.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image1.SetFrameType(i == 0 ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta);
    encoded_image1.SetSimulcastIndex(0);
    encoded_image1.qp_ = 10;
    encoded_image1._encodedWidth = kDefaultScaledWidth;
    encoded_image1._encodedHeight = kDefaultScaledHeight;

    EncodedImage encoded_image2;
    encoded_image2.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image2.SetFrameType(i == 0 ? VideoFrameType::kVideoFrameKey
                                       : VideoFrameType::kVideoFrameDelta);
    encoded_image2.SetSimulcastIndex(1);
    encoded_image2.qp_ = 10;
    encoded_image2._encodedWidth = kDefaultScaledWidth;
    encoded_image2._encodedHeight = kDefaultScaledHeight;

    generator.OnCapturedFrame(frame);

    std::optional<
        absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
        data1 = generator.OnEncodedImage(encoded_image1);

    std::optional<
        absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
        data2 = generator.OnEncodedImage(encoded_image2);

    if (i == 0) {
      ASSERT_TRUE(data1.has_value());
      ASSERT_TRUE(data2.has_value());
      ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data1));

      ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data2));

      EXPECT_TRUE(
          absl::get<FrameInstrumentationData>(*data1).communicate_upper_bits);
      EXPECT_TRUE(
          absl::get<FrameInstrumentationData>(*data2).communicate_upper_bits);
    } else if (data1.has_value() || data2.has_value()) {
      if (data1.has_value()) {
        ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data1));
        EXPECT_FALSE(
            absl::get<FrameInstrumentationData>(*data1).communicate_upper_bits);
      }
      if (data2.has_value()) {
        ASSERT_TRUE(absl::holds_alternative<FrameInstrumentationData>(*data2));
        EXPECT_FALSE(
            absl::get<FrameInstrumentationData>(*data2).communicate_upper_bits);
      }
      has_found_delta_frame = true;
    }
  }
  EXPECT_TRUE(has_found_delta_frame);
}

}  // namespace
}  // namespace webrtc
