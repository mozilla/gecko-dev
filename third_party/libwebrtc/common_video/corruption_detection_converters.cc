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
#include "rtc_base/checks.h"

namespace webrtc {

namespace {

int GetSequenceIndexForMessage(int sequence_index,
                               bool communicate_upper_bits) {
  return communicate_upper_bits ? (sequence_index >> 7)
                                : (sequence_index & 0b0111'1111);
}

}  // namespace

std::optional<CorruptionDetectionMessage>
ConvertFrameInstrumentationDataToCorruptionDetectionMessage(
    const FrameInstrumentationData& data) {
  if (data.sequence_index < 0 || data.sequence_index > 0b0011'1111'1111'1111) {
    return std::nullopt;
  }
  // Frame instrumentation data must have sample values.
  if (data.sample_values.empty()) {
    return std::nullopt;
  }
  return CorruptionDetectionMessage::Builder()
      .WithSequenceIndex(GetSequenceIndexForMessage(
          data.sequence_index, data.communicate_upper_bits))
      .WithInterpretSequenceIndexAsMostSignificantBits(
          data.communicate_upper_bits)
      .WithStdDev(data.std_dev)
      .WithLumaErrorThreshold(data.luma_error_threshold)
      .WithChromaErrorThreshold(data.chroma_error_threshold)
      .WithSampleValues(data.sample_values)
      .Build();
}

std::optional<CorruptionDetectionMessage>
ConvertFrameInstrumentationSyncDataToCorruptionDetectionMessage(
    const FrameInstrumentationSyncData& data) {
  RTC_DCHECK(data.communicate_upper_bits)
      << "FrameInstrumentationSyncData data must always send the upper bits.";

  if (data.sequence_index < 0 || data.sequence_index > 0b0011'1111'1111'1111) {
    return std::nullopt;
  }
  return CorruptionDetectionMessage::Builder()
      .WithSequenceIndex(GetSequenceIndexForMessage(
          data.sequence_index, data.communicate_upper_bits))
      .WithInterpretSequenceIndexAsMostSignificantBits(true)
      .Build();
}

}  // namespace webrtc
