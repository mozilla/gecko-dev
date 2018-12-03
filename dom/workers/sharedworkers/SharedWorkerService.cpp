/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerService.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SystemGroup.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex sSharedWorkerMutex;

// Raw pointer because SharedWorkerParent keeps this object alive.
SharedWorkerService* MOZ_NON_OWNING_REF sSharedWorkerService;

nsresult PopulateContentSecurityPolicy(
    nsIContentSecurityPolicy* aCSP,
    const nsTArray<ContentSecurityPolicy>& aPolicies) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCSP);
  MOZ_ASSERT(!aPolicies.IsEmpty());

  for (const ContentSecurityPolicy& policy : aPolicies) {
    nsresult rv = aCSP->AppendPolicy(policy.policy(), policy.reportOnlyFlag(),
                                     policy.deliveredViaMetaTagFlag());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult PopulatePrincipalContentSecurityPolicy(
    nsIPrincipal* aPrincipal, const nsTArray<ContentSecurityPolicy>& aPolicies,
    const nsTArray<ContentSecurityPolicy>& aPreloadPolicies) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (!aPolicies.IsEmpty()) {
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    aPrincipal->EnsureCSP(nullptr, getter_AddRefs(csp));
    nsresult rv = PopulateContentSecurityPolicy(csp, aPolicies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!aPreloadPolicies.IsEmpty()) {
    nsCOMPtr<nsIContentSecurityPolicy> preloadCsp;
    aPrincipal->EnsurePreloadCSP(nullptr, getter_AddRefs(preloadCsp));
    nsresult rv = PopulateContentSecurityPolicy(preloadCsp, aPreloadPolicies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

class GetOrCreateWorkerManagerRunnable final : public Runnable {
 public:
  GetOrCreateWorkerManagerRunnable(SharedWorkerParent* aActor,
                                   const RemoteWorkerData& aData,
                                   uint64_t aWindowID,
                                   const MessagePortIdentifier& aPortIdentifier)
      : Runnable("GetOrCreateWorkerManagerRunnable"),
        mBackgroundEventTarget(GetCurrentThreadEventTarget()),
        mActor(aActor),
        mData(aData),
        mWindowID(aWindowID),
        mPortIdentifier(aPortIdentifier) {}

  NS_IMETHOD
  Run() {
    // The service is always available because it's kept alive by the actor.
    SharedWorkerService* service = SharedWorkerService::Get();
    MOZ_ASSERT(service);

    service->GetOrCreateWorkerManagerOnMainThread(
        mBackgroundEventTarget, mActor, mData, mWindowID, mPortIdentifier);

    return NS_OK;
  }

 private:
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
  RefPtr<SharedWorkerParent> mActor;
  RemoteWorkerData mData;
  uint64_t mWindowID;
  MessagePortIdentifier mPortIdentifier;
};

class RemoveWorkerManagerRunnable final : public Runnable {
 public:
  RemoveWorkerManagerRunnable(SharedWorkerService* aService,
                              SharedWorkerManager* aManager)
      : Runnable("RemoveWorkerManagerRunnable"),
        mService(aService),
        mManager(aManager) {
    MOZ_ASSERT(mService);
    MOZ_ASSERT(mManager);
  }

  NS_IMETHOD
  Run() {
    mService->RemoveWorkerManagerOnMainThread(mManager);
    return NS_OK;
  }

 private:
  RefPtr<SharedWorkerService> mService;
  RefPtr<SharedWorkerManager> mManager;
};

class WorkerManagerCreatedRunnable final : public Runnable {
 public:
  WorkerManagerCreatedRunnable(SharedWorkerManager* aManager,
                               SharedWorkerParent* aActor,
                               const RemoteWorkerData& aData,
                               uint64_t aWindowID,
                               const MessagePortIdentifier& aPortIdentifier)
      : Runnable("WorkerManagerCreatedRunnable"),
        mManager(aManager),
        mActor(aActor),
        mData(aData),
        mWindowID(aWindowID),
        mPortIdentifier(aPortIdentifier) {}

  NS_IMETHOD
  Run() {
    AssertIsOnBackgroundThread();

    if (NS_WARN_IF(!mManager->MaybeCreateRemoteWorker(
            mData, mWindowID, mPortIdentifier, mActor->OtherPid()))) {
      mActor->ErrorPropagation(NS_ERROR_FAILURE);
      return NS_OK;
    }

    mManager->AddActor(mActor);
    mActor->ManagerCreated(mManager);
    return NS_OK;
  }

 private:
  RefPtr<SharedWorkerManager> mManager;
  RefPtr<SharedWorkerParent> mActor;
  RemoteWorkerData mData;
  uint64_t mWindowID;
  MessagePortIdentifier mPortIdentifier;
};

class ErrorPropagationRunnable final : public Runnable {
 public:
  ErrorPropagationRunnable(SharedWorkerParent* aActor, nsresult aError)
      : Runnable("ErrorPropagationRunnable"), mActor(aActor), mError(aError) {}

  NS_IMETHOD
  Run() {
    AssertIsOnBackgroundThread();
    mActor->ErrorPropagation(mError);
    return NS_OK;
  }

 private:
  RefPtr<SharedWorkerParent> mActor;
  nsresult mError;
};

}  // namespace

/* static */ already_AddRefed<SharedWorkerService>
SharedWorkerService::GetOrCreate() {
  AssertIsOnBackgroundThread();

  StaticMutexAutoLock lock(sSharedWorkerMutex);

  if (sSharedWorkerService) {
    RefPtr<SharedWorkerService> instance = sSharedWorkerService;
    return instance.forget();
  }

  RefPtr<SharedWorkerService> instance = new SharedWorkerService();
  return instance.forget();
}

/* static */ SharedWorkerService* SharedWorkerService::Get() {
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService);
  return sSharedWorkerService;
}

SharedWorkerService::SharedWorkerService() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(!sSharedWorkerService);
  sSharedWorkerService = this;
}

SharedWorkerService::~SharedWorkerService() {
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService == this);
  sSharedWorkerService = nullptr;
}

void SharedWorkerService::GetOrCreateWorkerManager(
    SharedWorkerParent* aActor, const RemoteWorkerData& aData,
    uint64_t aWindowID, const MessagePortIdentifier& aPortIdentifier) {
  AssertIsOnBackgroundThread();

  // The real check happens on main-thread.
  RefPtr<GetOrCreateWorkerManagerRunnable> r =
      new GetOrCreateWorkerManagerRunnable(aActor, aData, aWindowID,
                                           aPortIdentifier);

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  nsresult rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SharedWorkerService::GetOrCreateWorkerManagerOnMainThread(
    nsIEventTarget* aBackgroundEventTarget, SharedWorkerParent* aActor,
    const RemoteWorkerData& aData, uint64_t aWindowID,
    const MessagePortIdentifier& aPortIdentifier) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);

  RefPtr<SharedWorkerManager> manager;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aData.principalInfo(), &rv);
  if (NS_WARN_IF(!principal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  rv = PopulatePrincipalContentSecurityPolicy(principal, aData.principalCsp(),
                                              aData.principalPreloadCsp());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal =
      PrincipalInfoToPrincipal(aData.loadingPrincipalInfo(), &rv);
  if (NS_WARN_IF(!loadingPrincipal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  rv = PopulatePrincipalContentSecurityPolicy(
      loadingPrincipal, aData.loadingPrincipalCsp(),
      aData.loadingPrincipalPreloadCsp());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  // Let's see if there is already a SharedWorker to share.
  nsCOMPtr<nsIURI> resolvedScriptURL =
      DeserializeURI(aData.resolvedScriptURL());
  for (SharedWorkerManager* workerManager : mWorkerManagers) {
    if (workerManager->MatchOnMainThread(aData.domain(), resolvedScriptURL,
                                         aData.name(), loadingPrincipal)) {
      manager = workerManager;
      break;
    }
  }

  // Let's create a new one.
  if (!manager) {
    manager = new SharedWorkerManager(aBackgroundEventTarget, aData,
                                      loadingPrincipal);

    mWorkerManagers.AppendElement(manager);
  } else {
    // We are attaching the actor to an existing one.
    if (manager->IsSecureContext() != aData.isSecureContext()) {
      ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor,
                                   NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  RefPtr<WorkerManagerCreatedRunnable> r = new WorkerManagerCreatedRunnable(
      manager, aActor, aData, aWindowID, aPortIdentifier);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void SharedWorkerService::ErrorPropagationOnMainThread(
    nsIEventTarget* aBackgroundEventTarget, SharedWorkerParent* aActor,
    nsresult aError) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<ErrorPropagationRunnable> r =
      new ErrorPropagationRunnable(aActor, aError);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void SharedWorkerService::RemoveWorkerManager(SharedWorkerManager* aManager) {
  AssertIsOnBackgroundThread();

  // We pass 'this' in order to be kept alive.
  RefPtr<RemoveWorkerManagerRunnable> r =
      new RemoveWorkerManagerRunnable(this, aManager);

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  nsresult rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SharedWorkerService::RemoveWorkerManagerOnMainThread(
    SharedWorkerManager* aManager) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mWorkerManagers.Contains(aManager));

  mWorkerManagers.RemoveElement(aManager);
}

}  // namespace dom
}  // namespace mozilla
