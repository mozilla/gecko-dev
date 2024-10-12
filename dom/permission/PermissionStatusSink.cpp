/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionStatusSink.h"
#include "PermissionObserver.h"
#include "PermissionStatus.h"

#include "mozilla/Permission.h"
#include "mozilla/PermissionDelegateHandler.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"

namespace mozilla::dom {

PermissionStatusSink::PermissionStatusSink(PermissionStatus* aPermissionStatus,
                                           PermissionName aPermissionName,
                                           const nsACString& aPermissionType)
    : mSerialEventTarget(NS_GetCurrentThread()),
      mPermissionStatus(aPermissionStatus),
      mMutex("PermissionStatusSink::mMutex"),
      mPermissionName(aPermissionName),
      mPermissionType(aPermissionType) {
  MOZ_ASSERT(aPermissionStatus);
  MOZ_ASSERT(mSerialEventTarget);

  nsCOMPtr<nsIGlobalObject> global = aPermissionStatus->GetOwnerGlobal();
  if (NS_WARN_IF(!global)) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull();
  if (NS_WARN_IF(!principal)) {
    return;
  }

  mPrincipalForPermission = Permission::ClonePrincipalForPermission(principal);
}

PermissionStatusSink::~PermissionStatusSink() = default;

RefPtr<PermissionStatusSink::PermissionStatePromise>
PermissionStatusSink::Init() {
  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    MutexAutoLock lock(mMutex);

    mWorkerRef = WeakWorkerRef::Create(
        workerPrivate, [self = RefPtr(this)] { self->Disentangle(); });
  }

  return InvokeAsync(GetMainThreadSerialEventTarget(), __func__,
                     [self = RefPtr(this)] {
                       MOZ_ASSERT(!self->mObserver);

                       // Covers the onchange part
                       // Whenever the user agent is aware that the state of a
                       // PermissionStatus instance status has changed: ... (The
                       // observer calls PermissionChanged() to do the steps)
                       self->mObserver = PermissionObserver::GetInstance();
                       if (NS_WARN_IF(!self->mObserver)) {
                         return PermissionStatePromise::CreateAndReject(
                             NS_ERROR_FAILURE, __func__);
                       }

                       self->mObserver->AddSink(self);

                       // Covers the query part (Step 8.2 - 8.4)
                       return self->ComputeStateOnMainThread();
                     });
}

bool PermissionStatusSink::MaybeUpdatedByOnMainThread(
    nsIPermission* aPermission) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPrincipalForPermission) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> permissionPrincipal;
  aPermission->GetPrincipal(getter_AddRefs(permissionPrincipal));
  if (!permissionPrincipal) {
    return false;
  }

  return mPrincipalForPermission->Equals(permissionPrincipal);
}

bool PermissionStatusSink::MaybeUpdatedByNotifyOnlyOnMainThread(
    nsPIDOMWindowInner* aInnerWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  return false;
}

void PermissionStatusSink::PermissionChangedOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  ComputeStateOnMainThread()->Then(
      mSerialEventTarget, __func__,
      [self = RefPtr(this)](
          const PermissionStatePromise::ResolveOrRejectValue& aResult) {
        if (aResult.IsResolve() && self->mPermissionStatus) {
          self->mPermissionStatus->PermissionChanged(aResult.ResolveValue());
        }
      });
}

void PermissionStatusSink::Disentangle() {
  MOZ_ASSERT(mSerialEventTarget->IsOnCurrentThread());

  mPermissionStatus = nullptr;

  {
    MutexAutoLock lock(mMutex);
    mWorkerRef = nullptr;
  }

  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)] {
        if (self->mObserver) {
          self->mObserver->RemoveSink(self);
          self->mObserver = nullptr;
        }
      }));
}

RefPtr<PermissionStatusSink::PermissionStatePromise>
PermissionStatusSink::ComputeStateOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  // Step 1: If settings wasn't passed, set it to the current settings object.
  // Step 2: If settings is a non-secure context, return "denied".
  // XXX(krosylight): No such steps here, and no WPT coverage?

  // The permission handler covers the rest of the steps, although the model
  // does not exactly match what the spec has. (Not passing "permission key" for
  // example)

  if (mSerialEventTarget->IsOnCurrentThread()) {
    if (!mPermissionStatus) {
      return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
    }

    RefPtr<nsGlobalWindowInner> window = mPermissionStatus->GetOwnerWindow();
    return ComputeStateOnMainThreadInternal(window);
  }

  nsCOMPtr<nsPIDOMWindowInner> ancestorWindow;
  nsCOMPtr<nsIPrincipal> workerPrincipal;

  {
    MutexAutoLock lock(mMutex);

    if (!mWorkerRef) {
      // We have been disentangled.
      return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
    }

    // If we have mWorkerRef, we haven't received the WorkerRef notification
    // yet.
    WorkerPrivate* workerPrivate = mWorkerRef->GetUnsafePrivate();
    MOZ_ASSERT(workerPrivate);

    ancestorWindow = workerPrivate->GetAncestorWindow();
    workerPrincipal = workerPrivate->GetPrincipal();
  }

  if (ancestorWindow) {
    return ComputeStateOnMainThreadInternal(ancestorWindow);
  }

  if (NS_WARN_IF(!workerPrincipal)) {
    return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<nsIPermissionManager> permissionManager =
      PermissionManager::GetInstance();
  if (!permissionManager) {
    return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  uint32_t action = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permissionManager->TestPermissionFromPrincipal(
      workerPrincipal, mPermissionType, &action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return PermissionStatePromise::CreateAndReject(rv, __func__);
  }

  return PermissionStatePromise::CreateAndResolve(action, __func__);
}

RefPtr<PermissionStatusSink::PermissionStatePromise>
PermissionStatusSink::ComputeStateOnMainThreadInternal(
    nsPIDOMWindowInner* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aWindow)) {
    return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  RefPtr<Document> document = aWindow->GetExtantDoc();
  if (NS_WARN_IF(!document)) {
    return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  uint32_t action = nsIPermissionManager::DENY_ACTION;

  PermissionDelegateHandler* permissionHandler =
      document->GetPermissionDelegateHandler();
  if (NS_WARN_IF(!permissionHandler)) {
    return PermissionStatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  nsresult rv = permissionHandler->GetPermissionForPermissionsAPI(
      mPermissionType, &action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return PermissionStatePromise::CreateAndReject(rv, __func__);
  }

  return PermissionStatePromise::CreateAndResolve(action, __func__);
}

}  // namespace mozilla::dom
