/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnManagerBase.h"
#include "mozilla/dom/WebAuthnTransactionChild.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla::dom {

WebAuthnManagerBase::WebAuthnManagerBase(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
}

WebAuthnManagerBase::~WebAuthnManagerBase() { MOZ_ASSERT(NS_IsMainThread()); }

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthnManagerBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(WebAuthnManagerBase, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthnManagerBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthnManagerBase)

/***********************************************************************
 * IPC Protocol Implementation
 **********************************************************************/

bool WebAuthnManagerBase::MaybeCreateBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mActor) {
    return true;
  }

  RefPtr<WebAuthnTransactionChild> actor = new WebAuthnTransactionChild();

  WindowGlobalChild* windowGlobalChild = mParent->GetWindowGlobalChild();
  if (!windowGlobalChild ||
      !windowGlobalChild->SendPWebAuthnTransactionConstructor(child)) {
    return false;
  }

  mActor = actor;
  mActor->SetManager(this);

  return true;
}

void WebAuthnManagerBase::ActorDestroyed() {
  MOZ_ASSERT(NS_IsMainThread());
  mActor = nullptr;
}

}  // namespace mozilla::dom
