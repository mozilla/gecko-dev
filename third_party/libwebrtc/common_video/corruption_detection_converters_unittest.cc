/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/corruption_detection_converters.h"

#include <optional>

#include "common_video/corruption_detection_message.h"
#include "common_video/frame_instrumentation_data.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

TEST(CorruptionDetectionConvertersTest, ConvertsValidData) {
  FrameInstrumentationData data = {.sequence_index = 1,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {1.0, 2.0, 3.0, 4.0, 5.0}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->sequence_index(), 1);
  EXPECT_FALSE(message->interpret_sequence_index_as_most_significant_bits());
  EXPECT_EQ(message->std_dev(), 1.0);
  EXPECT_EQ(message->luma_error_threshold(), 5);
  EXPECT_EQ(message->chroma_error_threshold(), 5);
  EXPECT_THAT(message->sample_values(), ElementsAre(1.0, 2.0, 3.0, 4.0, 5.0));
}

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenSequenceIndexIsNegative) {
  FrameInstrumentationData data = {.sequence_index = -1,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {1.0, 2.0, 3.0, 4.0, 5.0}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenSequenceIndexIsTooLarge) {
  // Sequence index must be at max 14 bits.
  FrameInstrumentationData data = {.sequence_index = 0x4000,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {1.0, 2.0, 3.0, 4.0, 5.0}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenThereAreNoSampleValues) {
  // FrameInstrumentationData must by definition have at least one sample value.
  FrameInstrumentationData data = {.sequence_index = 1,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenNotSpecifyingSampleValues) {
  FrameInstrumentationData data = {.sequence_index = 1,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

TEST(CorruptionDetectionConvertersTest,
     ConvertsSequenceIndexWhenSetToUseUpperBits) {
  FrameInstrumentationData data = {.sequence_index = 0b0000'0110'0000'0101,
                                   .communicate_upper_bits = true,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {1.0, 2.0, 3.0, 4.0, 5.0}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->sequence_index(), 0b0000'1100);
  EXPECT_TRUE(message->interpret_sequence_index_as_most_significant_bits());
  EXPECT_EQ(message->std_dev(), 1.0);
  EXPECT_EQ(message->luma_error_threshold(), 5);
  EXPECT_EQ(message->chroma_error_threshold(), 5);
  EXPECT_THAT(message->sample_values(), ElementsAre(1.0, 2.0, 3.0, 4.0, 5.0));
}

TEST(CorruptionDetectionConvertersTest,
     ConvertsSequenceIndexWhenSetToUseLowerBits) {
  FrameInstrumentationData data = {.sequence_index = 0b0000'0110'0000'0101,
                                   .communicate_upper_bits = false,
                                   .std_dev = 1.0,
                                   .luma_error_threshold = 5,
                                   .chroma_error_threshold = 5,
                                   .sample_values = {1.0, 2.0, 3.0, 4.0, 5.0}};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationDataToCorruptionDetectionMessage(data);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->sequence_index(), 0b0000'0101);
  EXPECT_FALSE(message->interpret_sequence_index_as_most_significant_bits());
  EXPECT_EQ(message->std_dev(), 1.0);
  EXPECT_EQ(message->luma_error_threshold(), 5);
  EXPECT_EQ(message->chroma_error_threshold(), 5);
  EXPECT_THAT(message->sample_values(), ElementsAre(1.0, 2.0, 3.0, 4.0, 5.0));
}

TEST(CorruptionDetectionConvertersTest, ConvertsValidSyncData) {
  FrameInstrumentationSyncData data = {.sequence_index = 1,
                                       .communicate_upper_bits = true};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(data);
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(message->sequence_index(), 0);
  EXPECT_TRUE(message->interpret_sequence_index_as_most_significant_bits());
}

#if GTEST_HAS_DEATH_TEST
TEST(CorruptionDetectionConvertersTest, FailsWhenSetToNotCommunicateUpperBits) {
  FrameInstrumentationSyncData data = {.sequence_index = 1,
                                       .communicate_upper_bits = false};

  EXPECT_DEATH(
      ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(data), _);
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenSyncSequenceIndexIsNegative) {
  FrameInstrumentationSyncData data = {.sequence_index = -1,
                                       .communicate_upper_bits = true};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

TEST(CorruptionDetectionConvertersTest,
     ReturnsNulloptWhenSyncSequenceIndexIsTooLarge) {
  FrameInstrumentationSyncData data = {.sequence_index = 0x4000,
                                       .communicate_upper_bits = true};

  std::optional<CorruptionDetectionMessage> message =
      ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(data);
  ASSERT_FALSE(message.has_value());
}

}  // namespace
}  // namespace webrtc
