/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/payload_type_picker.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "api/rtc_error.h"
#include "media/base/codec.h"

namespace webrtc {

namespace {

// Note: The only fields we need from a Codec are the type (audio/video),
// the subtype (vp8/h264/....), the clock rate, the channel count, and the
// fmtp parameters. The use of cricket::Codec, which contains more fields,
// is only a temporary measure.

bool MatchesForSdp(const cricket::Codec& codec_1,
                   const cricket::Codec& codec_2) {
  return codec_1.name == codec_2.name && codec_1.type == codec_2.type &&
         codec_1.channels == codec_2.channels &&
         codec_1.clockrate == codec_2.clockrate &&
         codec_1.params == codec_2.params;
}

}  // namespace

RTCErrorOr<PayloadType> PayloadTypePicker::SuggestMapping(
    cricket::Codec codec) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

RTCError PayloadTypePicker::AddMapping(PayloadType payload_type,
                                       cricket::Codec codec) {
  return RTCError(RTCErrorType::UNSUPPORTED_OPERATION, "Not implemented yet");
}

RTCError PayloadTypeRecorder::AddMapping(PayloadType payload_type,
                                         cricket::Codec codec) {
  if (payload_type_to_codec_.find(payload_type) !=
      payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Attempt to insert duplicate mapping for PT");
  }
  payload_type_to_codec_.emplace(payload_type, codec);
  suggester_.AddMapping(payload_type, codec);
  return RTCError::OK();
}

std::vector<std::pair<PayloadType, cricket::Codec>>
PayloadTypeRecorder::GetMappings() {
  return std::vector<std::pair<PayloadType, cricket::Codec>>{};
}

RTCErrorOr<PayloadType> PayloadTypeRecorder::LookupPayloadType(
    cricket::Codec codec) {
  // Note that having multiple PTs mapping to the same codec is NOT an error.
  // In this case, we return the first found (not deterministic).
  auto result = std::find_if(
      payload_type_to_codec_.begin(), payload_type_to_codec_.end(),
      [codec](const auto& iter) { return MatchesForSdp(iter.second, codec); });
  if (result == payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "No payload type found for codec");
  }
  return result->first;
}

RTCErrorOr<cricket::Codec> PayloadTypeRecorder::LookupCodec(
    PayloadType payload_type) {
  auto result = payload_type_to_codec_.find(payload_type);
  if (result == payload_type_to_codec_.end()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER, "No such payload type");
  }
  return result->second;
}

void PayloadTypeRecorder::Checkpoint() {
  checkpoint_payload_type_to_codec_ = payload_type_to_codec_;
}
void PayloadTypeRecorder::Rollback() {
  payload_type_to_codec_ = checkpoint_payload_type_to_codec_;
}

}  // namespace webrtc
