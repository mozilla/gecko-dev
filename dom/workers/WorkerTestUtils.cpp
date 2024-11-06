/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ErrorResult.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerTestUtils.h"
#include "mozilla/dom/WorkerTestUtilsBinding.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

uint32_t WorkerTestUtils::CurrentTimerNestingLevel(const GlobalObject& aGlobal,
                                                   ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  return worker->GetCurrentTimerNestingLevel();
}

bool WorkerTestUtils::IsRunningInBackground(const GlobalObject&,
                                            ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  return worker->IsRunningInBackground();
}

namespace {

// Helper for HoldStrongWorkerRefUntilMainThreadObserverNotified that optionally
// holds a ThreadSafeWorkerRef until the given observer notification is notified
// and also notifies a monitor.
class WorkerTestUtilsObserver final : public nsIObserver {
 public:
  WorkerTestUtilsObserver(const nsACString& aTopic,
                          RefPtr<ThreadSafeWorkerRef>&& aWorkerRef)
      : mMonitor("WorkerTestUtils"),
        mTopic(aTopic),
        mWorkerRef(std::move(aWorkerRef)),
        mRegistered(false),
        mObserved(false) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    // We only register for one topic so we don't actually need to compare it.
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    MOZ_ALWAYS_SUCCEEDS(observerService->RemoveObserver(this, mTopic.get()));

    // The ThreadSafeWorkerRef is responsible for / knows how to drop the
    // underlying StrongWorkerRef on the worker.
    mWorkerRef = nullptr;

    MonitorAutoLock lock(mMonitor);
    mObserved = true;
    mMonitor.Notify();

    return NS_OK;
  }

  void Register() {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    MOZ_ALWAYS_SUCCEEDS(
        observerService->AddObserver(this, mTopic.get(), false));

    MonitorAutoLock lock(mMonitor);
    mRegistered = true;
    mMonitor.Notify();
  }

  void WaitOnRegister() {
    MonitorAutoLock lock(mMonitor);
    while (!mRegistered) {
      mMonitor.Wait();
    }
  }

  void WaitOnObserver() {
    MonitorAutoLock lock(mMonitor);
    while (!mObserved) {
      mMonitor.Wait();
    }
  }

 private:
  ~WorkerTestUtilsObserver() = default;

  Monitor mMonitor;
  nsAutoCString mTopic;
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  bool mRegistered;
  bool mObserved;
};

NS_IMPL_ISUPPORTS(WorkerTestUtilsObserver, nsIObserver)

}  // anonymous namespace

void WorkerTestUtils::HoldStrongWorkerRefUntilMainThreadObserverNotified(
    const GlobalObject&, const nsACString& aTopic, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<StrongWorkerRef> strongWorkerRef =
      StrongWorkerRef::Create(workerPrivate, "WorkerTestUtils");
  if (NS_WARN_IF(!strongWorkerRef)) {
    aErr.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<ThreadSafeWorkerRef> tsWorkerRef =
      new ThreadSafeWorkerRef(strongWorkerRef);

  auto observer =
      MakeRefPtr<WorkerTestUtilsObserver>(aTopic, std::move(tsWorkerRef));

  aErr = NS_DispatchToMainThread(NewRunnableMethod(
      "WorkerTestUtils::HoldStrongWorkerRefUntilMainThreadObserverNotified",
      observer, &WorkerTestUtilsObserver::Register));

  // Wait for the observer to be registered before returning control so that we
  // can be certain we won't miss an observer notification.
  observer->WaitOnRegister();
}

void WorkerTestUtils::BlockUntilMainThreadObserverNotified(
    const GlobalObject&, const nsACString& aTopic,
    WorkerTestCallback& aWhenObserving, ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto observer = MakeRefPtr<WorkerTestUtilsObserver>(aTopic, nullptr);

  aErr = NS_DispatchToMainThread(
      NewRunnableMethod("WorkerTestUtils::BlockUntilMainThreadObserverNotified",
                        observer, &WorkerTestUtilsObserver::Register));
  if (aErr.Failed()) {
    return;
  }

  observer->WaitOnRegister();

  aWhenObserving.Call(aErr);
  if (aErr.Failed()) {
    return;
  }

  observer->WaitOnObserver();
}

void WorkerTestUtils::NotifyObserverOnMainThread(const GlobalObject&,
                                                 const nsACString& aTopic,
                                                 ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());

  aErr = NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WorkerTestUtils::NotifyObserverOnMainThread",
      [topic = nsCString(aTopic)] {
        nsCOMPtr<nsIObserverService> observerService =
            services::GetObserverService();
        observerService->NotifyObservers(nullptr, topic.get(), nullptr);
      }));
}

}  // namespace mozilla::dom
