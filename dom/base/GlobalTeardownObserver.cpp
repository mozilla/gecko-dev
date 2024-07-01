/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GlobalTeardownObserver.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/dom/Document.h"

namespace mozilla {

GlobalTeardownObserver::GlobalTeardownObserver() = default;
GlobalTeardownObserver::GlobalTeardownObserver(nsIGlobalObject* aGlobalObject,
                                               bool aHasOrHasHadOwnerWindow)
    : mHasOrHasHadOwnerWindow(aHasOrHasHadOwnerWindow) {
  BindToOwner(aGlobalObject);
}

GlobalTeardownObserver::~GlobalTeardownObserver() {
  if (mParentObject) {
    mParentObject->RemoveGlobalTeardownObserver(this);
  }
}

nsGlobalWindowInner* GlobalTeardownObserver::GetOwnerWindow() const {
  return mHasOrHasHadOwnerWindow
             ? static_cast<nsGlobalWindowInner*>(mParentObject)
             : nullptr;
}

void GlobalTeardownObserver::BindToOwner(nsIGlobalObject* aOwner) {
  MOZ_ASSERT(!mParentObject);

  if (aOwner) {
    mParentObject = aOwner;
    aOwner->AddGlobalTeardownObserver(this);
    const bool isWindow = !!aOwner->GetAsInnerWindow();
    MOZ_ASSERT_IF(!isWindow, !mHasOrHasHadOwnerWindow);
    mHasOrHasHadOwnerWindow = isWindow;
  }
}

void GlobalTeardownObserver::DisconnectFromOwner() {
  if (mParentObject) {
    mParentObject->RemoveGlobalTeardownObserver(this);
    mParentObject = nullptr;
  }
}

nsresult GlobalTeardownObserver::CheckCurrentGlobalCorrectness() const {
  if (!mParentObject) {
    if (NS_IsMainThread() && !HasOrHasHadOwnerWindow()) {
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  // Main-thread.
  if (mHasOrHasHadOwnerWindow) {
    auto* ownerWin = static_cast<nsGlobalWindowInner*>(mParentObject);
    if (!ownerWin->IsCurrentInnerWindow()) {
      return NS_ERROR_FAILURE;
    }
  }

  if (mParentObject->IsDying() && !NS_IsMainThread()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

};  // namespace mozilla
