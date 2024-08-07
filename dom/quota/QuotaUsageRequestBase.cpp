/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaUsageRequestBase.h"

#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

void QuotaUsageRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (Canceled()) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult QuotaUsageRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  if (Cancel()) {
    NS_WARNING("Canceled more than once?!");
    return IPC_FAIL(this, "Request canceled more than once");
  }

  return IPC_OK();
}

}  // namespace mozilla::dom::quota
