/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerManagerService.h"
#include "mozilla/AppProcessChecker.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

namespace {

uint64_t sServiceWorkerManagerParentID = 0;

void
AssertIsInMainProcess()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
}

class RegisterServiceWorkerCallback final : public nsRunnable
{
public:
  RegisterServiceWorkerCallback(const ServiceWorkerRegistrationData& aData,
                                uint64_t aParentID)
    : mData(aData)
    , mParentID(aParentID)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    nsRefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->RegisterServiceWorker(mData);

    nsRefPtr<ServiceWorkerManagerService> managerService =
      ServiceWorkerManagerService::Get();
    if (managerService) {
      managerService->PropagateRegistration(mParentID, mData);
    }

    return NS_OK;
  }

private:
  ServiceWorkerRegistrationData mData;
  const uint64_t mParentID;
};

class UnregisterServiceWorkerCallback final : public nsRunnable
{
public:
  UnregisterServiceWorkerCallback(const PrincipalInfo& aPrincipalInfo,
                                  const nsString& aScope)
    : mPrincipalInfo(aPrincipalInfo)
    , mScope(aScope)
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();
  }

  NS_IMETHODIMP
  Run()
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    nsRefPtr<dom::ServiceWorkerRegistrar> service =
      dom::ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    service->UnregisterServiceWorker(mPrincipalInfo,
                                     NS_ConvertUTF16toUTF8(mScope));
    return NS_OK;
  }

private:
  const PrincipalInfo mPrincipalInfo;
  nsString mScope;
};

class CheckPrincipalWithCallbackRunnable final : public nsRunnable
{
public:
  CheckPrincipalWithCallbackRunnable(already_AddRefed<ContentParent> aParent,
                                     const PrincipalInfo& aPrincipalInfo,
                                     nsRunnable* aCallback)
    : mContentParent(aParent)
    , mPrincipalInfo(aPrincipalInfo)
    , mCallback(aCallback)
    , mBackgroundThread(NS_GetCurrentThread())
  {
    AssertIsInMainProcess();
    AssertIsOnBackgroundThread();

    MOZ_ASSERT(mContentParent);
    MOZ_ASSERT(mCallback);
    MOZ_ASSERT(mBackgroundThread);
  }

  NS_IMETHODIMP Run()
  {
    if (NS_IsMainThread()) {
      nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo);
      AssertAppPrincipal(mContentParent, principal);
      mContentParent = nullptr;

      mBackgroundThread->Dispatch(this, NS_DISPATCH_NORMAL);
      return NS_OK;
    }

    AssertIsOnBackgroundThread();
    mCallback->Run();
    mCallback = nullptr;

    return NS_OK;
  }

private:
  nsRefPtr<ContentParent> mContentParent;
  PrincipalInfo mPrincipalInfo;
  nsRefPtr<nsRunnable> mCallback;
  nsCOMPtr<nsIThread> mBackgroundThread;
};

} // anonymous namespace

ServiceWorkerManagerParent::ServiceWorkerManagerParent()
  : mService(ServiceWorkerManagerService::GetOrCreate())
  , mID(++sServiceWorkerManagerParentID)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this);
}

ServiceWorkerManagerParent::~ServiceWorkerManagerParent()
{
  AssertIsOnBackgroundThread();
}

bool
ServiceWorkerManagerParent::RecvRegister(
                                     const ServiceWorkerRegistrationData& aData)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aData.scope().IsEmpty() ||
      aData.scriptSpec().IsEmpty() ||
      aData.principal().type() == PrincipalInfo::TNullPrincipalInfo ||
      aData.principal().type() == PrincipalInfo::TSystemPrincipalInfo) {
    return false;
  }

  nsRefPtr<RegisterServiceWorkerCallback> callback =
    new RegisterServiceWorkerCallback(aData, mID);

  nsRefPtr<ContentParent> parent =
    BackgroundParent::GetContentParent(Manager());

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  nsRefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aData.principal(),
                                           callback);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return true;
}

bool
ServiceWorkerManagerParent::RecvUnregister(const PrincipalInfo& aPrincipalInfo,
                                           const nsString& aScope)
{
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // Basic validation.
  if (aScope.IsEmpty() ||
      aPrincipalInfo.type() == PrincipalInfo::TNullPrincipalInfo ||
      aPrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    return false;
  }

  nsRefPtr<UnregisterServiceWorkerCallback> callback =
    new UnregisterServiceWorkerCallback(aPrincipalInfo, aScope);

  nsRefPtr<ContentParent> parent =
    BackgroundParent::GetContentParent(Manager());

  // If the ContentParent is null we are dealing with a same-process actor.
  if (!parent) {
    callback->Run();
    return true;
  }

  nsRefPtr<CheckPrincipalWithCallbackRunnable> runnable =
    new CheckPrincipalWithCallbackRunnable(parent.forget(), aPrincipalInfo,
                                           callback);
  nsresult rv = NS_DispatchToMainThread(runnable);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateSoftUpdate(const OriginAttributes& aOriginAttributes,
                                                    const nsString& aScope)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateSoftUpdate(mID, aOriginAttributes, aScope);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateUnregister(const PrincipalInfo& aPrincipalInfo,
                                                    const nsString& aScope)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateUnregister(mID, aPrincipalInfo, aScope);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateRemove(const nsCString& aHost)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateRemove(mID, aHost);
  return true;
}

bool
ServiceWorkerManagerParent::RecvPropagateRemoveAll()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PropagateRemoveAll(mID);
  return true;
}

bool
ServiceWorkerManagerParent::RecvShutdown()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->UnregisterActor(this);
  mService = nullptr;

  unused << Send__delete__(this);
  return true;
}

void
ServiceWorkerManagerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (mService) {
    // This object is about to be released and with it, also mService will be
    // released too.
    mService->UnregisterActor(this);
  }
}

} // workers namespace
} // dom namespace
} // mozilla namespace
