/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaUsageRequestChild.h"

// Local includes
#include "QuotaRequests.h"

// Global includes
#include "mozilla/Assertions.h"

namespace mozilla::dom::quota {

QuotaUsageRequestChild::QuotaUsageRequestChild(UsageRequest* aRequest)
    : mRequest(aRequest) {
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(quota::QuotaUsageRequestChild);
}

QuotaUsageRequestChild::~QuotaUsageRequestChild() {
  // Can't assert owning thread here because the request is cleared.

  MOZ_COUNT_DTOR(quota::QuotaUsageRequestChild);
}

#ifdef DEBUG

void QuotaUsageRequestChild::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mRequest);
  mRequest->AssertIsOnOwningThread();
}

#endif  // DEBUG

mozilla::ipc::IPCResult QuotaUsageRequestChild::Recv__delete__() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRequest);

  return IPC_OK();
}

void QuotaUsageRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mRequest) {
    mRequest->ClearBackgroundActor();
#ifdef DEBUG
    mRequest = nullptr;
#endif
  }
}

}  // namespace mozilla::dom::quota
