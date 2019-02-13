/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerManagerService.h"
#include "ServiceWorkerManagerParent.h"
#include "ServiceWorkerRegistrar.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace workers {

namespace {

ServiceWorkerManagerService* sInstance = nullptr;

} // anonymous namespace

ServiceWorkerManagerService::ServiceWorkerManagerService()
{
  AssertIsOnBackgroundThread();

  // sInstance is a raw ServiceWorkerManagerService*.
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

ServiceWorkerManagerService::~ServiceWorkerManagerService()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this);
  MOZ_ASSERT(mAgents.Count() == 0);

  sInstance = nullptr;
}

/* static */ already_AddRefed<ServiceWorkerManagerService>
ServiceWorkerManagerService::Get()
{
  AssertIsOnBackgroundThread();

  nsRefPtr<ServiceWorkerManagerService> instance = sInstance;
  return instance.forget();
}

/* static */ already_AddRefed<ServiceWorkerManagerService>
ServiceWorkerManagerService::GetOrCreate()
{
  AssertIsOnBackgroundThread();

  nsRefPtr<ServiceWorkerManagerService> instance = sInstance;
  if (!instance) {
    instance = new ServiceWorkerManagerService();
  }
  return instance.forget();
}

void
ServiceWorkerManagerService::RegisterActor(ServiceWorkerManagerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mAgents.Contains(aParent));

  mAgents.PutEntry(aParent);
}

void
ServiceWorkerManagerService::UnregisterActor(ServiceWorkerManagerParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mAgents.Contains(aParent));

  mAgents.RemoveEntry(aParent);
}

namespace {

struct MOZ_STACK_CLASS RegistrationData final
{
  RegistrationData(ServiceWorkerRegistrationData& aData,
                   uint64_t aParentID)
    : mData(aData)
    , mParentID(aParentID)
#ifdef DEBUG
    , mParentFound(false)
#endif
  {
    MOZ_COUNT_CTOR(RegistrationData);
  }

  ~RegistrationData()
  {
    MOZ_COUNT_DTOR(RegistrationData);
  }

  const ServiceWorkerRegistrationData& mData;
  const uint64_t mParentID;
#ifdef DEBUG
  bool mParentFound;
#endif
};

PLDHashOperator
RegistrationEnumerator(nsPtrHashKey<ServiceWorkerManagerParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<RegistrationData*>(aPtr);

  ServiceWorkerManagerParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent->ID() != data->mParentID) {
    unused << parent->SendNotifyRegister(data->mData);
#ifdef DEBUG
  } else {
    data->mParentFound = true;
#endif
  }

  return PL_DHASH_NEXT;
}

struct MOZ_STACK_CLASS SoftUpdateData final
{
  SoftUpdateData(const OriginAttributes& aOriginAttributes,
                 const nsAString& aScope,
                 uint64_t aParentID)
    : mOriginAttributes(aOriginAttributes)
    , mScope(aScope)
    , mParentID(aParentID)
#ifdef DEBUG
    , mParentFound(false)
#endif
  {
    MOZ_COUNT_CTOR(SoftUpdateData);
  }

  ~SoftUpdateData()
  {
    MOZ_COUNT_DTOR(SoftUpdateData);
  }

  const OriginAttributes& mOriginAttributes;
  const nsString mScope;
  const uint64_t mParentID;
#ifdef DEBUG
  bool mParentFound;
#endif
};

PLDHashOperator
SoftUpdateEnumerator(nsPtrHashKey<ServiceWorkerManagerParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<SoftUpdateData*>(aPtr);
  ServiceWorkerManagerParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent->ID() != data->mParentID) {
    unused <<parent->SendNotifySoftUpdate(data->mOriginAttributes,
                                          data->mScope);
#ifdef DEBUG
  } else {
    data->mParentFound = true;
#endif
  }

  return PL_DHASH_NEXT;
}

struct MOZ_STACK_CLASS UnregisterData final
{
  UnregisterData(const PrincipalInfo& aPrincipalInfo,
                 const nsAString& aScope,
                 uint64_t aParentID)
    : mPrincipalInfo(aPrincipalInfo)
    , mScope(aScope)
    , mParentID(aParentID)
#ifdef DEBUG
    , mParentFound(false)
#endif
  {
    MOZ_COUNT_CTOR(UnregisterData);
  }

  ~UnregisterData()
  {
    MOZ_COUNT_DTOR(UnregisterData);
  }

  const PrincipalInfo mPrincipalInfo;
  const nsString mScope;
  const uint64_t mParentID;
#ifdef DEBUG
  bool mParentFound;
#endif
};

PLDHashOperator
UnregisterEnumerator(nsPtrHashKey<ServiceWorkerManagerParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<UnregisterData*>(aPtr);
  ServiceWorkerManagerParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent->ID() != data->mParentID) {
    unused << parent->SendNotifyUnregister(data->mPrincipalInfo, data->mScope);
#ifdef DEBUG
  } else {
    data->mParentFound = true;
#endif
  }

  return PL_DHASH_NEXT;
}

struct MOZ_STACK_CLASS RemoveAllData final
{
  explicit RemoveAllData(uint64_t aParentID)
    : mParentID(aParentID)
#ifdef DEBUG
    , mParentFound(false)
#endif
  {
    MOZ_COUNT_CTOR(RemoveAllData);
  }

  ~RemoveAllData()
  {
    MOZ_COUNT_DTOR(RemoveAllData);
  }

  const uint64_t mParentID;
#ifdef DEBUG
  bool mParentFound;
#endif
};

PLDHashOperator
RemoveAllEnumerator(nsPtrHashKey<ServiceWorkerManagerParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<RemoveAllData*>(aPtr);
  ServiceWorkerManagerParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent->ID() != data->mParentID) {
    unused << parent->SendNotifyRemoveAll();
#ifdef DEBUG
  } else {
    data->mParentFound = true;
#endif
  }

  return PL_DHASH_NEXT;
}

struct MOZ_STACK_CLASS RemoveData final
{
  RemoveData(const nsACString& aHost,
             uint64_t aParentID)
    : mHost(aHost)
    , mParentID(aParentID)
#ifdef DEBUG
    , mParentFound(false)
#endif
  {
    MOZ_COUNT_CTOR(RemoveData);
  }

  ~RemoveData()
  {
    MOZ_COUNT_DTOR(RemoveData);
  }

  const nsCString mHost;
  const uint64_t mParentID;
#ifdef DEBUG
  bool mParentFound;
#endif
};

PLDHashOperator
RemoveEnumerator(nsPtrHashKey<ServiceWorkerManagerParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<RemoveData*>(aPtr);
  ServiceWorkerManagerParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent->ID() != data->mParentID) {
    unused << parent->SendNotifyRemove(data->mHost);
#ifdef DEBUG
  } else {
    data->mParentFound = true;
#endif
  }

  return PL_DHASH_NEXT;
}

} // anonymous namespce

void
ServiceWorkerManagerService::PropagateRegistration(
                                           uint64_t aParentID,
                                           ServiceWorkerRegistrationData& aData)
{
  AssertIsOnBackgroundThread();

  RegistrationData data(aData, aParentID);
  mAgents.EnumerateEntries(RegistrationEnumerator, &data);

#ifdef DEBUG
  MOZ_ASSERT(data.mParentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateSoftUpdate(
                                      uint64_t aParentID,
                                      const OriginAttributes& aOriginAttributes,
                                      const nsAString& aScope)
{
  AssertIsOnBackgroundThread();

  SoftUpdateData data(aOriginAttributes, aScope, aParentID);
  mAgents.EnumerateEntries(SoftUpdateEnumerator, &data);

#ifdef DEBUG
  MOZ_ASSERT(data.mParentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateUnregister(
                                            uint64_t aParentID,
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsAString& aScope)
{
  AssertIsOnBackgroundThread();

  nsRefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  // It's possible that we don't have any ServiceWorkerManager managing this
  // scope but we still need to unregister it from the ServiceWorkerRegistrar.
  service->UnregisterServiceWorker(aPrincipalInfo,
                                   NS_ConvertUTF16toUTF8(aScope));

  UnregisterData data(aPrincipalInfo, aScope, aParentID);
  mAgents.EnumerateEntries(UnregisterEnumerator, &data);

#ifdef DEBUG
  MOZ_ASSERT(data.mParentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateRemove(uint64_t aParentID,
                                             const nsACString& aHost)
{
  AssertIsOnBackgroundThread();

  RemoveData data(aHost, aParentID);
  mAgents.EnumerateEntries(RemoveEnumerator, &data);

#ifdef DEBUG
  MOZ_ASSERT(data.mParentFound);
#endif
}

void
ServiceWorkerManagerService::PropagateRemoveAll(uint64_t aParentID)
{
  AssertIsOnBackgroundThread();

  nsRefPtr<dom::ServiceWorkerRegistrar> service =
    dom::ServiceWorkerRegistrar::Get();
  MOZ_ASSERT(service);

  service->RemoveAll();

  RemoveAllData data(aParentID);
  mAgents.EnumerateEntries(RemoveAllEnumerator, &data);

#ifdef DEBUG
  MOZ_ASSERT(data.mParentFound);
#endif
}

} // workers namespace
} // dom namespace
} // mozilla namespace
