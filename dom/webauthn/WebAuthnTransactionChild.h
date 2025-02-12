/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnTransactionChild_h
#define mozilla_dom_WebAuthnTransactionChild_h

#include "mozilla/dom/PWebAuthnTransactionChild.h"

/*
 * Child process IPC implementation for WebAuthn API. Receives results of
 * WebAuthn transactions from the parent process, and sends them to the
 * WebAuthnHandler either cancel the transaction, or be formatted and relayed to
 * content.
 */

namespace mozilla::dom {

class WebAuthnHandler;

class WebAuthnTransactionChild final : public PWebAuthnTransactionChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebAuthnTransactionChild, override);

  WebAuthnTransactionChild() = default;

  void ActorDestroy(ActorDestroyReason why) override;

  void SetHandler(WebAuthnHandler* aMananger);

 private:
  ~WebAuthnTransactionChild() = default;

  WebAuthnHandler* mHandler;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnTransactionChild_h
