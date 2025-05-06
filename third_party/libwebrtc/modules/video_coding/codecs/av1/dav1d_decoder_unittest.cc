/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/dav1d_decoder.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::NotNull;

class TestAv1Decoder : public DecodedImageCallback {
 public:
  using DecodeCallback =
      absl::AnyInvocable<void(const VideoFrame& decoded_frame)>;

  TestAv1Decoder() : decoder_(CreateDav1dDecoder()) {
    if (decoder_ == nullptr) {
      ADD_FAILURE() << "Failed to create decoder";
      return;
    }
    EXPECT_TRUE(decoder_->Configure({}));
    EXPECT_EQ(decoder_->RegisterDecodeCompleteCallback(this),
              WEBRTC_VIDEO_CODEC_OK);
  }
  // This class requires pointer stability and thus not copyable nor movable.
  TestAv1Decoder(const TestAv1Decoder&) = delete;
  TestAv1Decoder& operator=(const TestAv1Decoder&) = delete;

  void Decode(const EncodedImage& image, DecodeCallback callback = nullptr) {
    ASSERT_THAT(decoder_, NotNull());
    callback_ = std::move(callback);
    int32_t error =
        decoder_->Decode(image, /*render_time_ms=*/image.capture_time_ms_);
    if (error != WEBRTC_VIDEO_CODEC_OK) {
      ADD_FAILURE() << "Failed to decode frame with timestamp "
                    << image.RtpTimestamp() << " with error code " << error;
      return;
    }
  }

 private:
  int32_t Decoded(VideoFrame& decoded_frame) override {
    Decoded(decoded_frame, /*decode_time_ms=*/std::nullopt,
            /*qp=*/std::nullopt);
    return 0;
  }
  void Decoded(VideoFrame& decoded_frame,
               std::optional<int32_t> /*decode_time_ms*/,
               std::optional<uint8_t> /*qp*/) override {
    if (callback_) {
      callback_(decoded_frame);
      callback_ = nullptr;
    }
  }

  const std::unique_ptr<VideoDecoder> decoder_;
  DecodeCallback callback_;
};

TEST(Dav1dDecoderTest, DeliversRenderResolution) {
  // Verifies that dav1d decoder sets render resolution in decoded frame and
  // that the decoder wrapper removes padding.

  // AV1 bitstream containing a single frame of 36x20 encoded and 32x16 render
  // resolution.
  uint8_t data[] = {0x12, 0x00, 0x0a, 0x06, 0x18, 0x15, 0x23, 0x9f, 0x60,
                    0x10, 0x32, 0x18, 0x20, 0x03, 0xe0, 0x01, 0xf2, 0xb0,
                    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0xf2, 0x44,
                    0xd6, 0xa5, 0x3b, 0x7c, 0x8b, 0x7c, 0x8c, 0x6b, 0x9a};
  EncodedImage encoded_frame;
  encoded_frame.SetEncodedData(EncodedImageBuffer::Create(data, sizeof(data)));

  TestAv1Decoder decoder;
  int num_decoded_frames = 0;
  decoder.Decode(encoded_frame,
                 [&num_decoded_frames](const VideoFrame& decoded_frame) {
                   EXPECT_EQ(decoded_frame.width(), 32);
                   EXPECT_EQ(decoded_frame.height(), 16);
                   ++num_decoded_frames;
                 });
  EXPECT_EQ(num_decoded_frames, 1);
}

}  // namespace
}  // namespace webrtc
