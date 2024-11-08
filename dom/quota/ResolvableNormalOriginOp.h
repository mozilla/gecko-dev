/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_
#define DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_

#include "NormalOriginOperationBase.h"

#include "mozilla/MozPromise.h"
#include "mozilla/dom/FlippedOnce.h"

namespace mozilla::dom::quota {

template <typename ResolveValueT, bool IsExclusive>
class ResolvableNormalOriginOp : public NormalOriginOperationBase {
 public:
  NS_INLINE_DECL_REFCOUNTING(ResolvableNormalOriginOp, override)

  using PromiseType = MozPromise<ResolveValueT, nsresult, IsExclusive>;

  RefPtr<PromiseType> OnResults() {
    AssertIsOnOwningThread();

    return mPromiseHolder.Ensure(__func__);
  }

 protected:
  ResolvableNormalOriginOp(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                           const char* aName)
      : NormalOriginOperationBase(std::move(aQuotaManager), aName) {
    AssertIsOnOwningThread();
  }

  virtual ~ResolvableNormalOriginOp() = default;

  virtual ResolveValueT UnwrapResolveValue() = 0;

#ifdef DEBUG
  bool ResolveValueConsumed() { return mResolveValueConsumed; }
#endif

 private:
  void SendResults() override {
    if (Canceled()) {
      mResultCode = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(mResultCode)) {
      mPromiseHolder.ResolveIfExists(UnwrapResolveValue(), __func__);
#ifdef DEBUG
      mResolveValueConsumed.Flip();
#endif
    } else {
      mPromiseHolder.RejectIfExists(mResultCode, __func__);
    }
  }

  MozPromiseHolder<PromiseType> mPromiseHolder;
#ifdef DEBUG
  FlippedOnce<false> mResolveValueConsumed;
#endif
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_RESOLVABLENORMALORIGINOP_H_
