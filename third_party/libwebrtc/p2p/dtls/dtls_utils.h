/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_UTILS_H_
#define P2P_DTLS_DTLS_UTILS_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "api/array_view.h"

namespace cricket {

const size_t kDtlsRecordHeaderLen = 13;
const size_t kMaxDtlsPacketLen = 2048;

bool IsDtlsPacket(rtc::ArrayView<const uint8_t> payload);
bool IsDtlsClientHelloPacket(rtc::ArrayView<const uint8_t> payload);
bool IsDtlsHandshakePacket(rtc::ArrayView<const uint8_t> payload);

std::optional<std::vector<uint16_t>> GetDtlsHandshakeAcks(
    rtc::ArrayView<const uint8_t> dtls_packet);

}  // namespace cricket

#endif  // P2P_DTLS_DTLS_UTILS_H_
