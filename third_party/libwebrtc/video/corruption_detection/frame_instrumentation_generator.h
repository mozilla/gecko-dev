/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
#define VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_

#include <cstdint>
#include <map>
#include <queue>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "video/corruption_detection/halton_frame_sampler.h"

namespace webrtc {

// TODO: b/358039777 - Error handling: negative values etc.
struct FrameInstrumentationSyncData {
  int sequence_index;
  bool is_key_frame;
};

struct FrameInstrumentationData {
  int sequence_index;
  bool is_key_frame;
  double std_dev;
  int luma_error_threshold;
  int chroma_error_threshold;
  std::vector<double> sample_values;
};

class FrameInstrumentationGenerator {
 public:
  FrameInstrumentationGenerator() = delete;
  explicit FrameInstrumentationGenerator(VideoCodecType video_codec_type);

  FrameInstrumentationGenerator(const FrameInstrumentationGenerator&) = delete;
  FrameInstrumentationGenerator& operator=(
      const FrameInstrumentationGenerator&) = delete;

  ~FrameInstrumentationGenerator() = default;

  void OnCapturedFrame(VideoFrame frame);
  absl::optional<
      absl::variant<FrameInstrumentationSyncData, FrameInstrumentationData>>
  OnEncodedImage(const EncodedImage& encoded_image);

 private:
  struct Context {
    HaltonFrameSampler frame_sampler;
    uint32_t rtp_timestamp_of_last_key_frame = 0;
  };

  // Incoming video frames in capture order.
  std::queue<VideoFrame> captured_frames_;
  // Map from spatial or simulcast index to sampling context.
  std::map<int, Context> contexts_;
  const VideoCodecType video_codec_type_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
