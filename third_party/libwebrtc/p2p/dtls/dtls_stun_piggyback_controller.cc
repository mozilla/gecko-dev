/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/dtls/dtls_stun_piggyback_controller.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "p2p/dtls/dtls_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"

namespace cricket {

DtlsStunPiggybackController::DtlsStunPiggybackController(
    absl::AnyInvocable<void(rtc::ArrayView<const uint8_t>)> dtls_data_callback,
    absl::AnyInvocable<void()> disable_piggybacking_callback)
    : dtls_data_callback_(std::move(dtls_data_callback)),
      disable_piggybacking_callback_(std::move(disable_piggybacking_callback)) {
}

DtlsStunPiggybackController::~DtlsStunPiggybackController() {}

void DtlsStunPiggybackController::SetDtlsHandshakeComplete(bool is_dtls_client,
                                                           bool is_dtls13) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  // Peer does not support this so fallback to a normal DTLS handshake
  // happened.
  if (state_ == State::OFF) {
    return;
  }
  state_ = State::PENDING;
  // As DTLS 1.2 server we need to keep the last flight around until
  // we receive the post-handshake acknowledgment.
  // As DTLS 1.2 client we have nothing more to send at this point
  // but will continue to send ACK attributes until receiving
  // the last flight from the server.
  // For DTLS 1.3 this is reversed since the handshake has one round trip less.
  if ((is_dtls_client && !is_dtls13) || (!is_dtls_client && is_dtls13)) {
    pending_packet_.Clear();
  }
}

bool DtlsStunPiggybackController::MaybeConsumePacket(
    rtc::ArrayView<const uint8_t> data) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  bool should_consume =
      (state_ == State::TENTATIVE || state_ == State::CONFIRMED) &&
      IsDtlsPacket(data);
  if (should_consume) {
    // Note: this overwrites the existing packets which is an issue
    // if this gets called with fragmented DTLS flights.
    pending_packet_.SetData(data);
    return true;
  }
  return false;
}

void DtlsStunPiggybackController::ClearCachedPacketForTesting() {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  pending_packet_.Clear();
}

std::optional<absl::string_view>
DtlsStunPiggybackController::GetDataToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  RTC_DCHECK(stun_message_type == STUN_BINDING_REQUEST ||
             stun_message_type == STUN_BINDING_RESPONSE);
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  if (pending_packet_.size() == 0) {
    return std::nullopt;
  }
  return absl::string_view(pending_packet_);
}

std::optional<absl::string_view> DtlsStunPiggybackController::GetAckToPiggyback(
    StunMessageType stun_message_type) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return std::nullopt;
  }
  return handshake_ack_writer_.DataAsStringView();
}

void DtlsStunPiggybackController::ReportDataPiggybacked(
    const StunByteStringAttribute* data,
    const StunByteStringAttribute* ack) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Drop silently when receiving acked data when the peer previously did not
  // support or we already moved to the complete state.
  if (state_ == State::OFF || state_ == State::COMPLETE) {
    return;
  }

  // We sent dtls piggybacked but got nothing in return or
  // we received a stun request with neither attribute set
  // => peer does not support.
  if (state_ == State::TENTATIVE && data == nullptr && ack == nullptr) {
    state_ = State::OFF;
    pending_packet_.Clear();
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking not supported by peer.";
    disable_piggybacking_callback_();
    return;
  }

  // In PENDING state the peer may have stopped sending the ack
  // when it moved to the COMPLETE state. Move to the same state.
  if (state_ == State::PENDING && data == nullptr && ack == nullptr) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    pending_packet_.Clear();
    handshake_ack_writer_.Clear();
    handshake_messages_received_.clear();
    return;
  }

  // We sent dtls piggybacked and got something in return => peer does support.
  if (state_ == State::TENTATIVE) {
    state_ = State::CONFIRMED;
  }

  if (ack != nullptr && !ack->string_view().empty()) {
    RTC_LOG(LS_VERBOSE) << "DTLS-STUN piggybacking ACK: "
                        << rtc::hex_encode(ack->string_view());
  }
  // The response to the final flight of the handshake will not contain
  // the DTLS data but will contain an ack.
  // Must not happen on the initial server to client packet which
  // has no DTLS data yet.
  if (data == nullptr && ack != nullptr && state_ == State::PENDING) {
    RTC_LOG(LS_INFO) << "DTLS-STUN piggybacking complete.";
    state_ = State::COMPLETE;
    pending_packet_.Clear();
    handshake_ack_writer_.Clear();
    handshake_messages_received_.clear();
    return;
  }
  if (!data || data->length() == 0) {
    return;
  }

  // Extract the received message sequence numbers of the handshake
  // from the packet and prepare the ack to be sent.
  std::optional<std::vector<uint16_t>> new_message_sequences =
      GetDtlsHandshakeAcks(data->array_view());
  if (!new_message_sequences) {
    RTC_LOG(LS_ERROR) << "DTLS-STUN piggybacking failed to parse DTLS packet.";
    return;
  }
  if (!new_message_sequences->empty()) {
    for (const auto& message_seq : *new_message_sequences) {
      handshake_messages_received_.insert(message_seq);
    }
    handshake_ack_writer_.Clear();
    for (const auto& message_seq : handshake_messages_received_) {
      handshake_ack_writer_.WriteUInt16(message_seq);
    }
  }

  dtls_data_callback_(data->array_view());
}

void DtlsStunPiggybackController::SetEnabled(bool enabled) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  enabled_ = enabled;
  if (!enabled) {
    state_ = State::OFF;
  }
}

bool DtlsStunPiggybackController::enabled() const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  return enabled_;
}

}  // namespace cricket
