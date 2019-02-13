/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworker_h__
#define mozilla_dom_workers_serviceworker_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState.

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

namespace workers {

class ServiceWorkerInfo;
class ServiceWorkerManager;
class SharedWorker;

bool
ServiceWorkerVisible(JSContext* aCx, JSObject* aObj);

class ServiceWorker final : public DOMEventTargetHelper
{
  friend class ServiceWorkerManager;
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ServiceWorker, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(error)

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  ServiceWorkerState
  State() const
  {
    return mState;
  }

  void
  SetState(ServiceWorkerState aState)
  {
    mState = aState;
  }

  void
  GetScriptURL(nsString& aURL) const;

  void
  DispatchStateChange(ServiceWorkerState aState)
  {
    SetState(aState);
    DOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("statechange"));
  }

  void
  QueueStateChangeEvent(ServiceWorkerState aState);

#ifdef XP_WIN
#undef PostMessage
#endif

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  WorkerPrivate*
  GetWorkerPrivate() const;

private:
  // This class can only be created from the ServiceWorkerManager.
  ServiceWorker(nsPIDOMWindow* aWindow, ServiceWorkerInfo* aInfo,
                SharedWorker* aSharedWorker);

  // This class is reference-counted and will be destroyed from Release().
  ~ServiceWorker();

  ServiceWorkerState mState;
  const nsRefPtr<ServiceWorkerInfo> mInfo;

  // To allow ServiceWorkers to potentially drop the backing DOMEventTargetHelper and
  // re-instantiate it later, they simply own a SharedWorker member that
  // can be released and recreated as required rather than re-implement some of
  // the SharedWorker logic.
  nsRefPtr<SharedWorker> mSharedWorker;
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworker_h__
