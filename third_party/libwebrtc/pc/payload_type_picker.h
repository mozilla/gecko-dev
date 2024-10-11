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
#include <set>
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

class PayloadTypeRecorder;

class PayloadTypePicker {
 public:
  PayloadTypePicker();
  PayloadTypePicker(const PayloadTypePicker&) = delete;
  PayloadTypePicker& operator=(const PayloadTypePicker&) = delete;
  PayloadTypePicker(PayloadTypePicker&&) = delete;
  PayloadTypePicker& operator=(PayloadTypePicker&&) = delete;
  // Suggest a payload type for the codec.
  // If the excluder maps it to something different, don't suggest it.
  RTCErrorOr<PayloadType> SuggestMapping(cricket::Codec codec,
                                         PayloadTypeRecorder* excluder);
  RTCError AddMapping(PayloadType payload_type, cricket::Codec codec);

 private:
  class MapEntry {
   public:
    MapEntry(PayloadType payload_type, cricket::Codec codec)
        : payload_type_(payload_type), codec_(codec) {}
    PayloadType payload_type() const { return payload_type_; }
    cricket::Codec codec() const { return codec_; }

   private:
    PayloadType payload_type_;
    cricket::Codec codec_;
  };
  std::vector<MapEntry> entries_;
  std::set<PayloadType> seen_payload_types_;
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
