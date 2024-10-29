/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginOperationBase.h"

#include "mozilla/Assertions.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/quota/OriginOperationCallbacks.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/TargetPtrHolder.h"
#include "nsError.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

namespace {

template <class T>
void ResolveOrRejectCallback(const BoolPromise::ResolveOrRejectValue& aValue,
                             MozPromiseHolder<T>& aPromiseHolder) {
  if (aPromiseHolder.IsEmpty()) {
    return;
  }

  if constexpr (std::is_same_v<T, ExclusiveBoolPromise>) {
    aPromiseHolder.UseSynchronousTaskDispatch(__func__);
  }

  if (aValue.IsResolve()) {
    aPromiseHolder.Resolve(true, __func__);
  } else {
    aPromiseHolder.Reject(aValue.RejectValue(), __func__);
  }
}

}  // namespace

OriginOperationBase::OriginOperationBase(
    MovingNotNull<RefPtr<QuotaManager>>&& aQuotaManager, const char* aName)
    : BackgroundThreadObject(GetCurrentSerialEventTarget()),
      mQuotaManager(std::move(aQuotaManager)),
      mResultCode(NS_OK)
#ifdef QM_COLLECTING_OPERATION_TELEMETRY
      ,
      mName(aName)
#endif
{
  AssertIsOnOwningThread();
}

OriginOperationBase::~OriginOperationBase() { AssertIsOnOwningThread(); }

void OriginOperationBase::RunImmediately() {
  AssertIsOnOwningThread();

  [self = RefPtr(this)]() {
    if (QuotaManager::IsShuttingDown()) {
      return BoolPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
    }

    QM_TRY(MOZ_TO_RESULT(self->DoInit(*self->mQuotaManager)),
           CreateAndRejectBoolPromise);

    return self->Open();
  }()
#ifdef DEBUG
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr(this)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               // Give derived classes the occasion to add additional debug
               // only checks after the opening was successfully finished on
               // the current thread before passing the work to the IO thread.
               QM_TRY(MOZ_TO_RESULT(self->DirectoryOpen()),
                      CreateAndRejectBoolPromise);

               return BoolPromise::CreateAndResolve(true, __func__);
             })
#endif
      ->Then(mQuotaManager->IOThread(), __func__,
             [selfHolder = TargetPtrHolder(this)](
                 const BoolPromise::ResolveOrRejectValue& aValue) {
               if (aValue.IsReject()) {
                 return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                     __func__);
               }

               QM_TRY(MOZ_TO_RESULT(selfHolder->DoDirectoryWork(
                          *selfHolder->mQuotaManager)),
                      CreateAndRejectBoolPromise);

               uint32_t pauseOnIOThreadMs = StaticPrefs::
                   dom_quotaManager_originOperations_pauseOnIOThreadMs();
               if (pauseOnIOThreadMs > 0) {
                 PR_Sleep(PR_MillisecondsToInterval(pauseOnIOThreadMs));
               }

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self =
               RefPtr(this)](const BoolPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              MOZ_ASSERT(NS_SUCCEEDED(self->mResultCode));

              self->mResultCode = aValue.RejectValue();
            }

            ResolveOrRejectCallback(aValue, self->mWillFinishPromiseHolder);
            ResolveOrRejectCallback(aValue, self->mWillFinishSyncPromiseHolder);

            self->UnblockOpen();

            ResolveOrRejectCallback(aValue, self->mDidFinishPromiseHolder);
            ResolveOrRejectCallback(aValue, self->mDidFinishSyncPromiseHolder);
          });
}

OriginOperationCallbacks OriginOperationBase::GetCallbacks(
    const OriginOperationCallbackOptions& aCallbackOptions) {
  AssertIsOnOwningThread();

  return OriginOperationCallbackHolders::GetCallbacks(aCallbackOptions);
}

nsresult OriginOperationBase::DoInit(QuotaManager& aQuotaManager) {
  AssertIsOnOwningThread();

  return NS_OK;
}

#ifdef DEBUG
nsresult OriginOperationBase::DirectoryOpen() {
  AssertIsOnOwningThread();

  return NS_OK;
}
#endif

}  // namespace mozilla::dom::quota
