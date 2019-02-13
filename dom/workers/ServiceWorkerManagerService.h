/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerManagerService_h
#define mozilla_dom_ServiceWorkerManagerService_h

#include "nsISupportsImpl.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

namespace mozilla {

class OriginAttributes;

namespace ipc {
class PrincipalInfo;
}

namespace dom {

class ServiceWorkerRegistrationData;

namespace workers {

class ServiceWorkerManagerParent;

class ServiceWorkerManagerService final
{
public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerManagerService)

  static already_AddRefed<ServiceWorkerManagerService> Get();
  static already_AddRefed<ServiceWorkerManagerService> GetOrCreate();

  void RegisterActor(ServiceWorkerManagerParent* aParent);
  void UnregisterActor(ServiceWorkerManagerParent* aParent);

  void PropagateRegistration(uint64_t aParentID,
                             ServiceWorkerRegistrationData& aData);

  void PropagateSoftUpdate(uint64_t aParentID,
                           const OriginAttributes& aOriginAttributes,
                           const nsAString& aScope);

  void PropagateUnregister(uint64_t aParentID,
                           const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
                           const nsAString& aScope);

  void PropagateRemove(uint64_t aParentID, const nsACString& aHost);

  void PropagateRemoveAll(uint64_t aParentID);

private:
  ServiceWorkerManagerService();
  ~ServiceWorkerManagerService();

  nsTHashtable<nsPtrHashKey<ServiceWorkerManagerParent>> mAgents;
};

} // workers namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ServiceWorkerManagerService_h
