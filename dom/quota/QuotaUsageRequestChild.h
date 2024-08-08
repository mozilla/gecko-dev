/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAUSAGEREQUESTCHILD_H_
#define DOM_QUOTA_QUOTAUSAGEREQUESTCHILD_H_

#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/PQuotaUsageRequestChild.h"

namespace mozilla::dom::quota {

class QuotaManagerService;
class UsageRequest;

class QuotaUsageRequestChild final : public PQuotaUsageRequestChild {
  friend class QuotaChild;
  friend class QuotaManagerService;

  RefPtr<UsageRequest> mRequest;

 public:
  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  NS_INLINE_DECL_REFCOUNTING(QuotaUsageRequestChild, override)

  // IPDL methods are only called by IPDL.
  virtual mozilla::ipc::IPCResult Recv__delete__() override;

 private:
  // Only created by QuotaManagerService.
  explicit QuotaUsageRequestChild(UsageRequest* aRequest);

  // Only destroyed by QuotaChild.
  ~QuotaUsageRequestChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAUSAGEREQUESTCHILD_H_
