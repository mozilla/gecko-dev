/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/corruption_detection_extension.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "absl/container/inlined_vector.h"
#include "api/array_view.h"
#include "common_video/corruption_detection_message.h"

namespace webrtc {
namespace {

constexpr size_t kMandatoryPayloadBytes = 1;
constexpr size_t kConfigurationBytes = 3;
constexpr double kMaxValueForStdDev = 40.0;

}  // namespace

// The message format of the header extension:
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |B| seq# index  |  kernel size  | Y err | UV err|    sample 0   |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |    sample 1   |   sample 2    |    …   up to sample <=12
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// * B (1 bit): If the sequence number should be interpreted as the MSB or LSB
//   of the full size 14 bit sequence index described in the next point.
// * seq# index (7 bits): The index into the Halton sequence (used to locate
//   where the samples should be drawn from).
//   * If B is set: the 7 most significant bits of the true index. The 7 least
//     significant bits of the true index shall be interpreted as 0. This is
//     because this is the point where we can guarantee that the sender and
//     receiver has the same full index). For this reason, B must only be set
//     for key frames.
//   * If B is not set: The 7 LSB of the true index. The 7 most significant bits
//     should be inferred based on the most recent message.
// * kernel size (8 bits):  The standard deviation of the gaussian filter used
//   to weigh the samples. The value is scaled using a linear map:
//   0 = 0.0 to 255 = 40.0. A kernel size of 0 is interpreted as directly using
//   just the sample value at the desired coordinate, without any weighting.
// * Y err (4 bits): The allowed error for the luma channel.
// * UV err (4 bits): The allowed error for the chroma channels.
// * Sample N (8 bits): The N:th filtered sample from the input image. Each
//   sample represents a new point in one of the image planes, the plane and
//   coordinates being determined by index into the Halton sequence (starting at
//   seq# index and is incremented by one for each sample). Each sample has gone
//   through a Gaussian filter with the kernel size specified above. The samples
//   have been floored to the nearest integer.
//
// A special case is so called "synchronization" messages. These are messages
// that only contains the first byte. They always have B set and are used to
// keep the sender and receiver in sync even if no "full" messages have been
// sent for a while.

bool CorruptionDetectionExtension::Parse(rtc::ArrayView<const uint8_t> data,
                                         CorruptionDetectionMessage* message) {
  if (message == nullptr) {
    return false;
  }
  if ((data.size() != kMandatoryPayloadBytes &&
       data.size() <= kConfigurationBytes) ||
      data.size() > 16) {
    return false;
  }
  message->interpret_sequence_index_as_most_significant_bits_ = data[0] >> 7;
  message->sequence_index_ = data[0] & 0b0111'1111;
  if (data.size() == kMandatoryPayloadBytes) {
    return true;
  }
  message->std_dev_ = data[1] * kMaxValueForStdDev / 255.0;
  uint8_t channel_error_thresholds = data[2];
  message->luma_error_threshold_ = channel_error_thresholds >> 4;
  message->chroma_error_threshold_ = channel_error_thresholds & 0xF;
  message->sample_values_.assign(data.cbegin() + kConfigurationBytes,
                                 data.cend());
  return true;
}

bool CorruptionDetectionExtension::Write(
    rtc::ArrayView<uint8_t> data,
    const CorruptionDetectionMessage& message) {
  if (data.size() != ValueSize(message)) {
    return false;
  }

  data[0] = message.sequence_index() & 0b0111'1111;
  if (message.interpret_sequence_index_as_most_significant_bits()) {
    data[0] |= 0b1000'0000;
  }
  if (message.sample_values().empty()) {
    return true;
  }
  data[1] = static_cast<uint8_t>(
      std::round(message.std_dev() / kMaxValueForStdDev * 255.0));
  data[2] = (message.luma_error_threshold() << 4) |
            (message.chroma_error_threshold() & 0xF);
  rtc::ArrayView<uint8_t> sample_values = data.subview(kConfigurationBytes);
  for (size_t i = 0; i < message.sample_values().size(); ++i) {
    sample_values[i] = std::floor(message.sample_values()[i]);
  }
  return true;
}

size_t CorruptionDetectionExtension::ValueSize(
    const CorruptionDetectionMessage& message) {
  if (message.sample_values_.empty()) {
    return kMandatoryPayloadBytes;
  }
  return kConfigurationBytes + message.sample_values_.size();
}

}  // namespace webrtc
