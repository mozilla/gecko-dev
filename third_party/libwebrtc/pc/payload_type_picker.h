/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PAYLOAD_TYPE_PICKER_H_
#define PC_PAYLOAD_TYPE_PICKER_H_

#include <map>
#include <utility>
#include <vector>

#include "api/rtc_error.h"
#include "media/base/codec.h"
#include "rtc_base/strong_alias.h"

namespace webrtc {

class PayloadType : public StrongAlias<class PayloadTypeTag, uint8_t> {
 public:
  // Non-explicit conversions from and to ints are to be deprecated and
  // removed once calling code is upgraded.
  PayloadType(uint8_t pt) { value_ = pt; }                // NOLINT: explicit
  constexpr operator uint8_t() const& { return value_; }  // NOLINT: Explicit
};

class PayloadTypePicker {
 public:
  RTCErrorOr<PayloadType> SuggestMapping(cricket::Codec codec);
  RTCError AddMapping(PayloadType payload_type, cricket::Codec codec);
};

class PayloadTypeRecorder {
 public:
  explicit PayloadTypeRecorder(PayloadTypePicker& suggester)
      : suggester_(suggester) {}

  RTCError AddMapping(PayloadType payload_type, cricket::Codec codec);
  std::vector<std::pair<PayloadType, cricket::Codec>> GetMappings();
  RTCErrorOr<PayloadType> LookupPayloadType(cricket::Codec codec);
  RTCErrorOr<cricket::Codec> LookupCodec(PayloadType payload_type);
  // Transaction support.
  // Checkpoint() commits previous changes.
  void Checkpoint();
  // Rollback() rolls back to the previous checkpoint.
  void Rollback();

 private:
  PayloadTypePicker& suggester_;
  std::map<PayloadType, cricket::Codec> payload_type_to_codec_;
  std::map<PayloadType, cricket::Codec> checkpoint_payload_type_to_codec_;
};

}  // namespace webrtc

#endif  //  PC_PAYLOAD_TYPE_PICKER_H_
