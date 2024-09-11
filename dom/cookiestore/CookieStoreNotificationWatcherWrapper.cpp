/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CookieStoreNotificationWatcherWrapper.h"
#include "CookieStoreNotificationWatcher.h"
#include "CookieStore.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "nsProxyRelease.h"

namespace mozilla::dom {

// static
already_AddRefed<CookieStoreNotificationWatcherWrapper>
CookieStoreNotificationWatcherWrapper::Create(CookieStore* aCookieStore) {
  MOZ_ASSERT(aCookieStore);

  nsIPrincipal* principal = nullptr;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    principal = workerPrivate->GetPrincipal();
  } else {
    nsCOMPtr<nsPIDOMWindowInner> window = aCookieStore->GetOwnerWindow();
    MOZ_ASSERT(window);

    nsCOMPtr<Document> document = window->GetExtantDoc();
    if (NS_WARN_IF(!document)) {
      return nullptr;
    }

    principal = document->NodePrincipal();
  }

  if (NS_WARN_IF(!principal)) {
    return nullptr;
  }

  RefPtr<CookieStoreNotificationWatcherWrapper> wrapper =
      new CookieStoreNotificationWatcherWrapper();

  bool privateBrowsing = principal->OriginAttributesRef().IsPrivateBrowsing();

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [wrapper, privateBrowsing] {
          wrapper->CreateWatcherOnMainThread(privateBrowsing);
        }));
  } else {
    wrapper->CreateWatcherOnMainThread(privateBrowsing);
  }

  return wrapper.forget();
}

CookieStoreNotificationWatcherWrapper::
    ~CookieStoreNotificationWatcherWrapper() {
  CookieStoreNotificationWatcher::ReleaseOnMainThread(
      mWatcherOnMainThread.forget());
}

void CookieStoreNotificationWatcherWrapper::CreateWatcherOnMainThread(
    bool aPrivateBrowsing) {
  MOZ_ASSERT(NS_IsMainThread());
  mWatcherOnMainThread =
      CookieStoreNotificationWatcher::Create(aPrivateBrowsing);
}

void CookieStoreNotificationWatcherWrapper::ForgetOperationID(
    const nsID& aOperationID) {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [self = RefPtr(this), aOperationID] {
          self->ForgetOperationID(aOperationID);
        }));
    return;
  }

  if (mWatcherOnMainThread) {
    mWatcherOnMainThread->ForgetOperationID(aOperationID);
  }
}

void CookieStoreNotificationWatcherWrapper::ResolvePromiseWhenNotified(
    const nsID& aOperationID, Promise* aPromise) {
  MOZ_ASSERT(aPromise);

  class PromiseResolver final : public Runnable {
   public:
    explicit PromiseResolver(Promise* aPromise)
        : Runnable("CookieStoreNotificationWatcherWrapper::PromiseResolver"),
          mPromise(aPromise),
          mEventTarget(GetCurrentSerialEventTarget()) {}

    NS_IMETHOD Run() override {
      mPromise->MaybeResolveWithUndefined();
      mPromise = nullptr;
      return NS_OK;
    }

    bool HasPromise() const { return !!mPromise; }

   private:
    ~PromiseResolver() {
      NS_ProxyRelease(
          "CookieStoreNotificationWatcherWrapper::PromiseResolver::mPromise",
          mEventTarget, mPromise.forget());
    }

    RefPtr<Promise> mPromise;
    RefPtr<nsISerialEventTarget> mEventTarget;
  };

  RefPtr<PromiseResolver> resolver(new PromiseResolver(aPromise));

  RefPtr<ThreadSafeWorkerRef> workerRef;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<StrongWorkerRef> strongWorkerRef = StrongWorkerRef::Create(
        workerPrivate, "CookieStoreNotificationWatcher::PromiseResolver",
        [resolver = RefPtr(resolver)]() { resolver->Run(); });

    workerRef = new ThreadSafeWorkerRef(strongWorkerRef);
  }

  auto callback = [resolver = RefPtr(resolver),
                   eventTarget = RefPtr(GetCurrentSerialEventTarget()),
                   workerRef = RefPtr(workerRef)] {
    if (resolver->HasPromise()) {
      RefPtr<Runnable> runnable(resolver);
      eventTarget->Dispatch(runnable.forget());
    }
  };

  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        __func__, [self = RefPtr(this), callback, aOperationID] {
          self->mWatcherOnMainThread->CallbackWhenNotified(aOperationID,
                                                           callback);
        }));
    return;
  }

  if (mWatcherOnMainThread) {
    mWatcherOnMainThread->CallbackWhenNotified(aOperationID, callback);
  }
}

}  // namespace mozilla::dom
