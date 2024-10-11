/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StorageAccessPermissionStatus.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "nsGlobalWindowInner.h"
#include "nsIPermissionManager.h"
#include "PermissionStatusSink.h"

namespace mozilla::dom {

class StorageAccessPermissionStatusSink final : public PermissionStatusSink {
  Mutex mWorkerRefMutex;

  // Protected by mutex.
  // Created and released on worker-thread. Used also on main-thread.
  RefPtr<WeakWorkerRef> mWeakWorkerRef MOZ_GUARDED_BY(mWorkerRefMutex);

 public:
  StorageAccessPermissionStatusSink(PermissionStatus* aPermissionStatus,
                                    PermissionName aPermissionName,
                                    const nsACString& aPermissionType)
      : PermissionStatusSink(aPermissionStatus, aPermissionName,
                             aPermissionType),
        mWorkerRefMutex("StorageAccessPermissionStatusSink::mWorkerRefMutex") {}

  void Init() {
    if (!NS_IsMainThread()) {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      MutexAutoLock lock(mWorkerRefMutex);

      mWeakWorkerRef =
          WeakWorkerRef::Create(workerPrivate, [self = RefPtr(this)]() {
            MutexAutoLock lock(self->mWorkerRefMutex);
            self->mWeakWorkerRef = nullptr;
          });
    }
  }

 protected:
  bool MaybeUpdatedByOnMainThread(nsIPermission* aPermission) override {
    return false;
  }

  bool MaybeUpdatedByNotifyOnlyOnMainThread(
      nsPIDOMWindowInner* aInnerWindow) override {
    NS_ENSURE_TRUE(aInnerWindow, false);

    if (!mPermissionStatus) {
      return false;
    }

    nsCOMPtr<nsPIDOMWindowInner> ownerWindow;

    if (mSerialEventTarget->IsOnCurrentThread()) {
      ownerWindow = mPermissionStatus->GetOwnerWindow();
    } else {
      MutexAutoLock lock(mWorkerRefMutex);

      if (!mWeakWorkerRef) {
        return false;
      }

      // If we have mWeakWorkerRef, we haven't received the WorkerRef
      // notification yet.
      WorkerPrivate* workerPrivate = mWeakWorkerRef->GetUnsafePrivate();
      MOZ_ASSERT(workerPrivate);

      ownerWindow = workerPrivate->GetAncestorWindow();
    }

    NS_ENSURE_TRUE(ownerWindow, false);

    return ownerWindow->WindowID() == aInnerWindow->WindowID();
  }

  RefPtr<PermissionStatePromise> ComputeStateOnMainThread() override {
    if (mSerialEventTarget->IsOnCurrentThread()) {
      if (!mPermissionStatus) {
        return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
      }

      nsGlobalWindowInner* window = mPermissionStatus->GetOwnerWindow();
      if (NS_WARN_IF(!window)) {
        return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
      }

      WindowGlobalChild* wgc = window->GetWindowGlobalChild();
      if (NS_WARN_IF(!wgc)) {
        return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
      }

      // Perform a Permission Policy Request
      if (!FeaturePolicyUtils::IsFeatureAllowed(window->GetExtantDoc(),
                                                u"storage-access"_ns)) {
        return PermissionStatePromise::CreateAndResolve(
            nsIPermissionManager::PROMPT_ACTION, __func__);
      }

      return wgc->SendGetStorageAccessPermission(false)->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr(this)](uint32_t aAction) {
            // We never reveal PermissionState::Denied here
            return PermissionStatePromise::CreateAndResolve(
                aAction == nsIPermissionManager::ALLOW_ACTION
                    ? aAction
                    : nsIPermissionManager::PROMPT_ACTION,
                __func__);
          },
          [](mozilla::ipc::ResponseRejectReason aError) {
            return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                           __func__);
          });
    }

    // For workers we already have the correct value in workerPrivate.
    return InvokeAsync(mSerialEventTarget, __func__, [self = RefPtr(this)] {
      if (!self->mPermissionStatus) {
        return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
      }

      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      return PermissionStatePromise::CreateAndResolve(
          workerPrivate->StorageAccess() == StorageAccess::eAllow
              ? nsIPermissionManager::ALLOW_ACTION
              : nsIPermissionManager::PROMPT_ACTION,
          __func__);
    });
  }
};

StorageAccessPermissionStatus::StorageAccessPermissionStatus(
    nsIGlobalObject* aGlobal)
    : PermissionStatus(aGlobal, PermissionName::Storage_access) {}

already_AddRefed<PermissionStatusSink>
StorageAccessPermissionStatus::CreateSink() {
  RefPtr<StorageAccessPermissionStatusSink> sink =
      new StorageAccessPermissionStatusSink(this, Name(), GetPermissionType());
  sink->Init();
  return sink.forget();
}

}  // namespace mozilla::dom
