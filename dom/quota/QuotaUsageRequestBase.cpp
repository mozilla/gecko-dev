/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaUsageRequestBase.h"

#include "mozilla/dom/quota/PQuotaRequest.h"

namespace mozilla::dom::quota {

QuotaUsageRequestBase::~QuotaUsageRequestBase() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActorDestroyed);
}

RefPtr<BoolPromise> QuotaUsageRequestBase::OnCancel() {
  AssertIsOnOwningThread();

  return mCancelPromiseHolder.Ensure(__func__);
}

void QuotaUsageRequestBase::Destroy() {
  AssertIsOnOwningThread();

  if (!IsActorDestroyed()) {
    (void)PQuotaUsageRequestParent::Send__delete__(this, NS_OK);
  }
}

void QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();

  mCancelPromiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
}

mozilla::ipc::IPCResult QuotaUsageRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  mCancelPromiseHolder.ResolveIfExists(true, __func__);

  return IPC_OK();
}

}  // namespace mozilla::dom::quota
