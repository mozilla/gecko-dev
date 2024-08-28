/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/config/encoder_stream_factory.h"

#include "call/adaptation/video_source_restrictions.h"
#include "rtc_base/experiments/min_video_bitrate_experiment.h"
#include "test/explicit_key_value_config.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::cricket::EncoderStreamFactory;
using test::ExplicitKeyValueConfig;
using ::testing::IsEmpty;
using ::testing::Not;

std::vector<Resolution> GetStreamResolutions(
    const std::vector<VideoStream>& streams) {
  std::vector<Resolution> res;
  for (const auto& s : streams) {
    if (s.active) {
      res.push_back(
          {rtc::checked_cast<int>(s.width), rtc::checked_cast<int>(s.height)});
    }
  }
  return res;
}

VideoStream LayerWithRequestedResolution(Resolution res) {
  VideoStream s;
  s.requested_resolution = res;
  return s;
}

}  // namespace

TEST(EncoderStreamFactory, SinglecastRequestedResolution) {
  ExplicitKeyValueConfig field_trials("");
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.push_back(
      LayerWithRequestedResolution({.width = 640, .height = 360}));
  auto streams =
      factory->CreateEncoderStreams(field_trials, 1280, 720, encoder_config);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 640, .height = 360},
                                           }));
}

TEST(EncoderStreamFactory, SinglecastRequestedResolutionWithAdaptation) {
  ExplicitKeyValueConfig field_trials("");
  VideoSourceRestrictions restrictions(
      /* max_pixels_per_frame= */ (320 * 320),
      /* target_pixels_per_frame= */ absl::nullopt,
      /* max_frame_rate= */ absl::nullopt);
  VideoEncoder::EncoderInfo encoder_info;
  auto factory =
      rtc::make_ref_counted<EncoderStreamFactory>(encoder_info, restrictions);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 1;
  encoder_config.simulcast_layers.push_back(
      LayerWithRequestedResolution({.width = 640, .height = 360}));
  auto streams =
      factory->CreateEncoderStreams(field_trials, 1280, 720, encoder_config);
  EXPECT_EQ(streams[0].requested_resolution,
            (Resolution{.width = 640, .height = 360}));
  EXPECT_EQ(GetStreamResolutions(streams), (std::vector<Resolution>{
                                               {.width = 320, .height = 180},
                                           }));
}

TEST(EncoderStreamFactory, BitratePriority) {
  constexpr double kBitratePriority = 0.123;
  ExplicitKeyValueConfig field_trials("");
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.bitrate_priority = kBitratePriority;
  encoder_config.simulcast_layers = {
      LayerWithRequestedResolution({.width = 320, .height = 180}),
      LayerWithRequestedResolution({.width = 640, .height = 360})};
  auto streams =
      factory->CreateEncoderStreams(field_trials, 640, 360, encoder_config);
  ASSERT_EQ(streams.size(), 2u);
  EXPECT_EQ(streams[0].bitrate_priority, kBitratePriority);
  EXPECT_FALSE(streams[1].bitrate_priority);
}

TEST(EncoderStreamFactory, SetsMinBitrateToDefaultValue) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  auto streams = factory->CreateEncoderStreams(ExplicitKeyValueConfig(""), 1920,
                                               1080, encoder_config);
  ASSERT_THAT(streams, Not(IsEmpty()));
  EXPECT_EQ(streams[0].min_bitrate_bps, kDefaultMinVideoBitrateBps);
}

TEST(EncoderStreamFactory, SetsMinBitrateToExperimentalValue) {
  VideoEncoder::EncoderInfo encoder_info;
  auto factory = rtc::make_ref_counted<EncoderStreamFactory>(encoder_info);
  VideoEncoderConfig encoder_config;
  encoder_config.number_of_streams = 2;
  encoder_config.simulcast_layers.resize(encoder_config.number_of_streams);
  auto streams = factory->CreateEncoderStreams(
      ExplicitKeyValueConfig("WebRTC-Video-MinVideoBitrate/Enabled,br:1kbps/"),
      1920, 1080, encoder_config);
  ASSERT_THAT(streams, Not(IsEmpty()));
  EXPECT_NE(streams[0].min_bitrate_bps, kDefaultMinVideoBitrateBps);
  EXPECT_EQ(streams[0].min_bitrate_bps, 1000);
}
}  // namespace webrtc
