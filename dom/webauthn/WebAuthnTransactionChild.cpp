/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnHandler.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"

namespace mozilla::dom {

void WebAuthnTransactionChild::SetHandler(WebAuthnHandler* aHandler) {
  mHandler = aHandler;
}

void WebAuthnTransactionChild::ActorDestroy(ActorDestroyReason why) {
  // Called by either a __delete__ message from the parent, or when the
  // channel disconnects. Clear out the child actor reference to be sure.
  if (mHandler) {
    mHandler->ActorDestroyed();
    mHandler = nullptr;
  }
}

}  // namespace mozilla::dom
