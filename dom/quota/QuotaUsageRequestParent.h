/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAUSAGEREQUESTPARENT_H_
#define DOM_QUOTA_QUOTAUSAGEREQUESTPARENT_H_

#include "mozilla/dom/quota/BackgroundThreadObject.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"

namespace mozilla::dom::quota {

class QuotaUsageRequestParent : public BackgroundThreadObject,
                                public PQuotaUsageRequestParent {
 public:
  QuotaUsageRequestParent() = default;

  NS_INLINE_DECL_REFCOUNTING(QuotaUsageRequestParent, override)

  RefPtr<BoolPromise> OnCancel();

  void Destroy();

  // IPDL methods.
  mozilla::ipc::IPCResult RecvCancel();

 private:
  virtual ~QuotaUsageRequestParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  MozPromiseHolder<BoolPromise> mCancelPromiseHolder;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAUSAGEREQUESTPARENT_H_
