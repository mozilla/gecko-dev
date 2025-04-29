/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportFlowControl.h"

namespace mozilla::net {

Maybe<CapsuleEncoder>
SenderFlowControlStreamType::CreateStreamsBlockedCapsule() {
  auto blockedNeeded = BlockedNeeded();
  if (!blockedNeeded) {
    return Nothing();
  }

  Capsule capsule = Capsule::WebTransportStreamsBlocked(
      *blockedNeeded, mType == WebTransportStreamType::BiDi);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  BlockedSent();
  return Some(encoder);
}

Maybe<CapsuleEncoder>
SenderFlowControlStreamId::CreateStreamDataBlockedCapsule() {
  auto blockedNeeded = BlockedNeeded();
  if (!blockedNeeded) {
    return Nothing();
  }

  Capsule capsule = Capsule::WebTransportStreamDataBlocked(*blockedNeeded, mId);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  BlockedSent();
  return Some(encoder);
}

Maybe<CapsuleEncoder>
SenderFlowControlSession::CreateSessionDataBlockedCapsule() {
  auto blockedNeeded = BlockedNeeded();
  if (!blockedNeeded) {
    return Nothing();
  }

  Capsule capsule = Capsule::WebTransportDataBlocked(*blockedNeeded);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  BlockedSent();
  return Some(encoder);
}

Maybe<CapsuleEncoder> ReceiverFlowControlStreamType::CreateMaxStreamsCapsule() {
  if (!CapsuleNeeded()) {
    return Nothing();
  }

  uint64_t maxStreams = NextLimit();
  Capsule capsule = Capsule::WebTransportMaxStreams(
      maxStreams, mType == WebTransportStreamType::BiDi);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  CapsuleSent(maxStreams);
  return Some(encoder);
}

Maybe<CapsuleEncoder>
ReceiverFlowControlStreamId::CreateMaxStreamDataCapsule() {
  if (!CapsuleNeeded()) {
    return Nothing();
  }

  uint64_t maxAllowed = NextLimit();
  Capsule capsule = Capsule::WebTransportMaxStreamData(maxAllowed, mId);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  CapsuleSent(maxAllowed);
  return Some(encoder);
}

Maybe<CapsuleEncoder> ReceiverFlowControlSession::CreateMaxDataCapsule() {
  if (!CapsuleNeeded()) {
    return Nothing();
  }

  uint64_t maxAllowed = NextLimit();
  Capsule capsule = Capsule::WebTransportMaxData(maxAllowed);
  CapsuleEncoder encoder;
  encoder.EncodeCapsule(capsule);
  CapsuleSent(maxAllowed);
  return Some(encoder);
}

}  // namespace mozilla::net
