/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
#define P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <set>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

// This class is not thread safe; all methods must be called on the same thread
// as the constructor.
class DtlsStunPiggybackController {
 public:
  // dtls_data_callback will be called with any DTLS packets received
  // piggybacked.
  DtlsStunPiggybackController(
      absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)>
          dtls_data_callback);
  ~DtlsStunPiggybackController();

  // Initially set from IceConfig.dtls_handshake_in_stun
  // but is also set to FALSE before restarting handshake.
  void SetEnabled(bool enabled);
  bool enabled() const;

  enum class State {
    // We don't know if peer support DTLS piggybacked in STUN.
    // We will piggyback DTLS until we get a piggybacked response
    // or a STUN response with piggyback support.
    TENTATIVE = 0,
    // The peer supports DTLS in STUN and we continue the handshake.
    CONFIRMED = 1,
    // We are waiting for the final ack. Semantic differs depending
    // on DTLS role.
    PENDING = 2,
    // We successfully completed the DTLS handshake in STUN.
    COMPLETE = 3,
    // The peer does not support piggybacking DTLS in STUN.
    OFF = 4,
  };

  State state() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return state_;
  }

  // Called by DtlsTransport when handshake is complete.
  void SetDtlsHandshakeComplete(bool is_dtls_client, bool is_dtls13);

  // Intercepts DTLS packets which should go into the STUN packets during the
  // handshake.
  bool MaybeConsumePacket(rtc::ArrayView<const uint8_t> data);
  void ClearCachedPacketForTesting();

  // Called by Connection, when sending a STUN BINDING { REQUEST / RESPONSE }
  // to obtain optional DTLS data or ACKs.
  std::optional<absl::string_view> GetDataToPiggyback(
      StunMessageType stun_message_type);
  std::optional<absl::string_view> GetAckToPiggyback(
      StunMessageType stun_message_type);

  // Called by Connection when receiving a STUN BINDING { REQUEST / RESPONSE }.
  void ReportDataPiggybacked(const StunByteStringAttribute* data,
                             const StunByteStringAttribute* ack);

 private:
  bool enabled_ RTC_GUARDED_BY(sequence_checker_) = false;
  State state_ RTC_GUARDED_BY(sequence_checker_) = State::TENTATIVE;
  rtc::Buffer pending_packet_ RTC_GUARDED_BY(sequence_checker_);
  absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> dtls_data_callback_;

  std::set<uint16_t> handshake_messages_received_
      RTC_GUARDED_BY(sequence_checker_);
  rtc::ByteBufferWriter handshake_ack_writer_ RTC_GUARDED_BY(sequence_checker_);

  // In practice this will be the network thread.
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;
};

}  // namespace cricket

#endif  // P2P_DTLS_DTLS_STUN_PIGGYBACK_CONTROLLER_H_
