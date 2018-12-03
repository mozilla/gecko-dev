/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerImpl_h
#define mozilla_dom_ServiceWorkerImpl_h

#include "ServiceWorker.h"

namespace mozilla {
namespace dom {

class ServiceWorkerInfo;
class ServiceWorkerRegistrationInfo;

class ServiceWorkerImpl final : public ServiceWorker::Inner {
  RefPtr<ServiceWorkerInfo> mInfo;
  RefPtr<ServiceWorkerRegistrationInfo> mReg;
  ServiceWorker* mOuter;

  ~ServiceWorkerImpl();

  // ServiceWorker::Inner interface
  void AddServiceWorker(ServiceWorker* aWorker) override;

  void RemoveServiceWorker(ServiceWorker* aWorker) override;

  void GetRegistration(ServiceWorkerRegistrationCallback&& aSuccessCB,
                       ServiceWorkerFailureCallback&& aFailureCB) override;

  void PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
                   const ClientInfo& aClientInfo,
                   const ClientState& aClientState) override;

 public:
  ServiceWorkerImpl(ServiceWorkerInfo* aInfo,
                    ServiceWorkerRegistrationInfo* aReg);

  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerImpl, override);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ServiceWorkerImpl_h
