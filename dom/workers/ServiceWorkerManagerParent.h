/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerManagerParent_h
#define mozilla_dom_ServiceWorkerManagerParent_h

#include "mozilla/dom/PServiceWorkerManagerParent.h"

namespace mozilla {

class OriginAttributes;

namespace ipc {
class BackgroundParentImpl;
}

namespace dom {
namespace workers {

class ServiceWorkerManagerService;

class ServiceWorkerManagerParent final : public PServiceWorkerManagerParent
{
  friend class mozilla::ipc::BackgroundParentImpl;

public:
  uint64_t ID() const
  {
    return mID;
  }

private:
  ServiceWorkerManagerParent();
  ~ServiceWorkerManagerParent();

  virtual bool RecvRegister(
                           const ServiceWorkerRegistrationData& aData) override;

  virtual bool RecvUnregister(const PrincipalInfo& aPrincipalInfo,
                              const nsString& aScope) override;

  virtual bool RecvPropagateSoftUpdate(const OriginAttributes& aOriginAttributes,
                                       const nsString& aScope) override;

  virtual bool RecvPropagateUnregister(const PrincipalInfo& aPrincipalInfo,
                                       const nsString& aScope) override;

  virtual bool RecvPropagateRemove(const nsCString& aHost) override;

  virtual bool RecvPropagateRemoveAll() override;

  virtual bool RecvShutdown() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsRefPtr<ServiceWorkerManagerService> mService;

  // We use this ID in the Service in order to avoid the sending of messages to
  // ourself.
  uint64_t mID;
};

} // workers namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ServiceWorkerManagerParent_h
