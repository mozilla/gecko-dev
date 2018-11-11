/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/configurable_frame_size_encoder.h"

#include <string.h>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {
namespace test {

ConfigurableFrameSizeEncoder::ConfigurableFrameSizeEncoder(
    size_t max_frame_size)
    : callback_(NULL),
      max_frame_size_(max_frame_size),
      current_frame_size_(max_frame_size),
      buffer_(new uint8_t[max_frame_size]) {
  memset(buffer_.get(), 0, max_frame_size);
}

ConfigurableFrameSizeEncoder::~ConfigurableFrameSizeEncoder() {}

int32_t ConfigurableFrameSizeEncoder::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Encode(
    const I420VideoFrame& inputImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const std::vector<VideoFrameType>* frame_types) {
  EncodedImage encodedImage(
      buffer_.get(), current_frame_size_, max_frame_size_);
  encodedImage._completeFrame = true;
  encodedImage._encodedHeight = inputImage.height();
  encodedImage._encodedWidth = inputImage.width();
  encodedImage._frameType = kKeyFrame;
  encodedImage._timeStamp = inputImage.timestamp();
  encodedImage.capture_time_ms_ = inputImage.render_time_ms();
  RTPFragmentationHeader* fragmentation = NULL;
  CodecSpecificInfo specific;
  memset(&specific, 0, sizeof(specific));
  callback_->Encoded(encodedImage, &specific, fragmentation);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::SetChannelParameters(uint32_t packet_loss,
                                                           int64_t rtt) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::SetRates(uint32_t new_bit_rate,
                                               uint32_t frame_rate) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::SetPeriodicKeyFrames(bool enable) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::CodecConfigParameters(uint8_t* buffer,
                                                            int32_t size) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t ConfigurableFrameSizeEncoder::SetFrameSize(size_t size) {
  assert(size <= max_frame_size_);
  current_frame_size_ = size;
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace test
}  // namespace webrtc
