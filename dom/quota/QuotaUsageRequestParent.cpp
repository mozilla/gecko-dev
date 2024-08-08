/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaUsageRequestParent.h"

namespace mozilla::dom::quota {

QuotaUsageRequestParent::~QuotaUsageRequestParent() {
  AssertIsOnOwningThread();
}

RefPtr<BoolPromise> QuotaUsageRequestParent::OnCancel() {
  AssertIsOnOwningThread();

  return mCancelPromiseHolder.Ensure(__func__);
}

void QuotaUsageRequestParent::Destroy() {
  AssertIsOnOwningThread();

  if (CanSend()) {
    (void)PQuotaUsageRequestParent::Send__delete__(this);
  }
}

mozilla::ipc::IPCResult QuotaUsageRequestParent::RecvCancel() {
  AssertIsOnOwningThread();

  mCancelPromiseHolder.ResolveIfExists(true, __func__);

  return IPC_OK();
}

void QuotaUsageRequestParent::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  mCancelPromiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
}

}  // namespace mozilla::dom::quota
