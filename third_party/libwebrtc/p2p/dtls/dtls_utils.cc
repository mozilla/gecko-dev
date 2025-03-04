/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_utils.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "api/array_view.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"

namespace {
// https://datatracker.ietf.org/doc/html/rfc5246#appendix-A.1
const uint8_t kDtlsChangeCipherSpecRecord = 20;
const uint8_t kDtlsHandshakeRecord = 22;

// https://www.rfc-editor.org/rfc/rfc9147.html#section-4
const uint8_t kFixedBitmask = 0b00100000;
const uint8_t kConnectionBitmask = 0b00010000;
const uint8_t kSequenceNumberBitmask = 0b00001000;
const uint8_t kLengthPresentBitmask = 0b00000100;
}  // namespace

namespace cricket {

bool IsDtlsPacket(rtc::ArrayView<const uint8_t> payload) {
  const uint8_t* u = payload.data();
  return (payload.size() >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}

bool IsDtlsClientHelloPacket(rtc::ArrayView<const uint8_t> payload) {
  if (!IsDtlsPacket(payload)) {
    return false;
  }
  const uint8_t* u = payload.data();
  return payload.size() > 17 && u[0] == kDtlsHandshakeRecord && u[13] == 1;
}

bool IsDtlsHandshakePacket(rtc::ArrayView<const uint8_t> payload) {
  if (!IsDtlsPacket(payload)) {
    return false;
  }
  // change cipher spec is not a handshake packet. This used
  // to work because it was aggregated with the session ticket
  // which is no more. It is followed by the encrypted handshake
  // message which starts with a handshake record (22) again.
  return payload.size() > 17 && (payload[0] == kDtlsHandshakeRecord ||
                                 payload[0] == kDtlsChangeCipherSpecRecord);
}

// Returns a (unsorted) list of (msg_seq) received as part of the handshake.
std::optional<std::vector<uint16_t>> GetDtlsHandshakeAcks(
    rtc::ArrayView<const uint8_t> dtls_packet) {
  std::vector<uint16_t> acks;
  rtc::ByteBufferReader record_buf(dtls_packet);
  // https://datatracker.ietf.org/doc/html/rfc6347#section-4.1
  while (record_buf.Length() >= kDtlsRecordHeaderLen) {
    uint8_t content_type;
    uint64_t epoch_and_seq;
    uint16_t len;
    // Read content_type(1).
    if (!record_buf.ReadUInt8(&content_type)) {
      return std::nullopt;
    }

    // DTLS 1.3 rules:
    // https://www.rfc-editor.org/rfc/rfc9147.html#section-4.1
    if ((content_type & kFixedBitmask) == kFixedBitmask) {
      // Interpret as DTLSCipherText:
      // https://www.rfc-editor.org/rfc/rfc9147.html#appendix-A.1
      // We assume no connection id is used so C must be 0.
      if ((content_type & kConnectionBitmask) != 0) {
        return std::nullopt;
      }
      // Skip sequence_number(1 or 2 bytes depending on S bit).
      if (!record_buf.Consume((content_type & kSequenceNumberBitmask) ==
                                      kSequenceNumberBitmask
                                  ? 2
                                  : 1)) {
        return std::nullopt;
      }
      // If the L bit is set, consume the 16 bit length field.
      if ((content_type & kLengthPresentBitmask) == kLengthPresentBitmask) {
        if (!(record_buf.ReadUInt16(&len) && record_buf.Consume(len))) {
          return std::nullopt;
        }
      }
      // DTLSCipherText is encrypted so we can not read it.
      continue;
    }
    // Skip version(2), read epoch+seq(2+6), read len(2)
    if (!(record_buf.Consume(2) && record_buf.ReadUInt64(&epoch_and_seq) &&
          record_buf.ReadUInt16(&len) && record_buf.Length() >= len)) {
      return std::nullopt;
    }
    if (content_type != kDtlsHandshakeRecord) {
      record_buf.Consume(len);
      continue;
    }
    // Epoch 1+ is encrypted so we can not parse it.
    if (epoch_and_seq >> 6 != 0) {
      record_buf.Consume(len);
      continue;
    }

    // https://www.rfc-editor.org/rfc/rfc6347.html#section-4.2.2
    rtc::ByteBufferReader handshake_buf(record_buf.DataView().subview(0, len));
    while (handshake_buf.Length() > 0) {
      uint16_t msg_seq;
      uint32_t fragment_len;
      uint32_t fragment_offset;
      // Skip msg_type(1) and length(3), read msg_seq(2), skip
      // fragment_offset(3), read fragment_length(3) and consume it.
      if (!(handshake_buf.Consume(1 + 3) &&
            handshake_buf.ReadUInt16(&msg_seq) &&
            handshake_buf.ReadUInt24(&fragment_offset) &&
            handshake_buf.ReadUInt24(&fragment_len) &&
            handshake_buf.Consume(fragment_len))) {
        return std::nullopt;
      }
      acks.push_back(msg_seq);
      // Advance outer buffer.
      record_buf.Consume(12 + fragment_len);
    }
    RTC_DCHECK(handshake_buf.Length() == 0);
  }
  // Should have consumed everything.
  if (record_buf.Length() != 0) {
    return std::nullopt;
  }
  return acks;
}

}  // namespace cricket
