/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerUpdaterChild_h
#define mozilla_dom_ServiceWorkerUpdaterChild_h

#include "mozilla/dom/PServiceWorkerUpdaterChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

class ServiceWorkerUpdaterChild final : public PServiceWorkerUpdaterChild {
 public:
  ServiceWorkerUpdaterChild(GenericPromise* aPromise,
                            CancelableRunnable* aSuccessRunnable,
                            CancelableRunnable* aFailureRunnable);

  mozilla::ipc::IPCResult RecvProceed(const bool& aAllowed) override;

 private:
  void ActorDestroy(ActorDestroyReason aWhy) override;

  MozPromiseRequestHolder<GenericPromise> mPromiseHolder;

  RefPtr<CancelableRunnable> mSuccessRunnable;
  RefPtr<CancelableRunnable> mFailureRunnable;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ServiceWorkerUpdaterChild_h
