/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/base64.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "rtc_base/third_party/base64/base64.h"

namespace webrtc {

std::string Base64Encode(ArrayView<const uint8_t> data) {
  std::string result;
  Base64::EncodeFromArray(data.data(), data.size(), &result);
  return result;
}

std::optional<std::vector<uint8_t>> Base64Decode(absl::string_view data,
                                                 Base64DecodeOptions options) {
  Base64::DecodeFlags flags;
  switch (options) {
    case Base64DecodeOptions::kForgiving:
      flags =
          Base64::DO_PARSE_WHITE | Base64::DO_PAD_ANY | Base64::DO_TERM_BUFFER;
      break;
    case Base64DecodeOptions::kStrict:
      flags = Base64::DO_STRICT;
      break;
  }

  std::vector<uint8_t> result;
  if (!Base64::DecodeFromArray(data.data(), data.size(), flags, &result,
                               nullptr)) {
    return std::nullopt;
  }
  return result;
}

}  // namespace webrtc
