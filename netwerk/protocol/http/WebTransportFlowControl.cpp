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

}  // namespace mozilla::net
