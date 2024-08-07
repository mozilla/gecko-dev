/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_
#define DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_

#include "NormalOriginOperationBase.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"

namespace mozilla::dom::quota {

class UsageRequestResponse;

class QuotaUsageRequestBase : public NormalOriginOperationBase,
                              public PQuotaUsageRequestParent {
 protected:
  QuotaUsageRequestBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                        const char* aName)
      : NormalOriginOperationBase(std::move(aQuotaManager), aName) {}

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(UsageRequestResponse& aResponse) = 0;

 private:
  void SendResults() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvCancel() final;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAUSAGEREQUESTBASE_H_
