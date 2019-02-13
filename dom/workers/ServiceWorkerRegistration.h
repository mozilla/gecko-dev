/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistration_h
#define mozilla_dom_ServiceWorkerRegistration_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/ServiceWorkerBinding.h"
#include "mozilla/dom/ServiceWorkerCommon.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class Promise;
class PushManager;
class WorkerListener;

namespace workers {
class ServiceWorker;
class WorkerPrivate;
}

bool
ServiceWorkerRegistrationVisible(JSContext* aCx, JSObject* aObj);

// This class exists solely so that we can satisfy some WebIDL Func= attribute
// constraints. Func= converts the function name to a header file to include, in
// this case "ServiceWorkerRegistration.h".
class ServiceWorkerRegistration final
{
public:
  // Something that we can feed into the Func webidl property to ensure that
  // SetScope is never exposed to the user.
  static bool
  WebPushMethodHider(JSContext* unusedContext, JSObject* unusedObject) {
    return false;
  }

};

// Used by ServiceWorkerManager to notify ServiceWorkerRegistrations of
// updatefound event and invalidating ServiceWorker instances.
class ServiceWorkerRegistrationListener
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;

  virtual void
  UpdateFound() = 0;

  virtual void
  InvalidateWorkers(WhichServiceWorker aWhichOnes) = 0;

  virtual void
  GetScope(nsAString& aScope) const = 0;
};

class ServiceWorkerRegistrationBase : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(updatefound)

  ServiceWorkerRegistrationBase(nsPIDOMWindow* aWindow,
                                const nsAString& aScope);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetInstalling() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetWaiting() = 0;

  virtual already_AddRefed<workers::ServiceWorker>
  GetActive() = 0;

protected:
  virtual ~ServiceWorkerRegistrationBase()
  { }

  const nsString mScope;
};

class ServiceWorkerRegistrationMainThread final : public ServiceWorkerRegistrationBase,
                                                  public ServiceWorkerRegistrationListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationMainThread,
                                           ServiceWorkerRegistrationBase)

  ServiceWorkerRegistrationMainThread(nsPIDOMWindow* aWindow,
                                      const nsAString& aScope);

  void
  Update();

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<workers::ServiceWorker>
  GetInstalling() override;

  already_AddRefed<workers::ServiceWorker>
  GetWaiting() override;

  already_AddRefed<workers::ServiceWorker>
  GetActive() override;

  already_AddRefed<PushManager>
  GetPushManager(ErrorResult& aRv);

  // DOMEventTargethelper
  void DisconnectFromOwner() override
  {
    StopListeningForEvents();
    ServiceWorkerRegistrationBase::DisconnectFromOwner();
  }

  // ServiceWorkerRegistrationListener
  void
  UpdateFound() override;

  void
  InvalidateWorkers(WhichServiceWorker aWhichOnes) override;

  void
  GetScope(nsAString& aScope) const override
  {
    aScope = mScope;
  }

private:
  ~ServiceWorkerRegistrationMainThread();

  already_AddRefed<workers::ServiceWorker>
  GetWorkerReference(WhichServiceWorker aWhichOne);

  void
  StartListeningForEvents();

  void
  StopListeningForEvents();

  bool mListeningForEvents;

  // The following properties are cached here to ensure JS equality is satisfied
  // instead of acquiring a new worker instance from the ServiceWorkerManager
  // for every access. A null value is considered a cache miss.
  // These three may change to a new worker at any time.
  nsRefPtr<workers::ServiceWorker> mInstallingWorker;
  nsRefPtr<workers::ServiceWorker> mWaitingWorker;
  nsRefPtr<workers::ServiceWorker> mActiveWorker;

#ifndef MOZ_SIMPLEPUSH
  nsRefPtr<PushManager> mPushManager;
#endif
};

class ServiceWorkerRegistrationWorkerThread final : public ServiceWorkerRegistrationBase
                                                  , public workers::WorkerFeature
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorkerRegistrationWorkerThread,
                                           ServiceWorkerRegistrationBase)

  ServiceWorkerRegistrationWorkerThread(workers::WorkerPrivate* aWorkerPrivate,
                                        const nsAString& aScope);

  void
  Update();

  already_AddRefed<Promise>
  Unregister(ErrorResult& aRv);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<workers::ServiceWorker>
  GetInstalling() override;

  already_AddRefed<workers::ServiceWorker>
  GetWaiting() override;

  already_AddRefed<workers::ServiceWorker>
  GetActive() override;

  void
  GetScope(nsAString& aScope) const
  {
    aScope = mScope;
  }

  bool
  Notify(JSContext* aCx, workers::Status aStatus) override;

private:
  enum Reason
  {
    RegistrationIsGoingAway = 0,
    WorkerIsGoingAway,
  };

  ~ServiceWorkerRegistrationWorkerThread();

  void
  InitListener();

  void
  ReleaseListener(Reason aReason);

  workers::WorkerPrivate* mWorkerPrivate;
  nsRefPtr<WorkerListener> mListener;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_ServiceWorkerRegistration_h */
