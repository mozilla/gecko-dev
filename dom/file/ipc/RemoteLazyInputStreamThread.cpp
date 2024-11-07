/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteLazyInputStreamThread.h"

#include "ErrorList.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsXPCOMPrivate.h"

using namespace mozilla::ipc;

namespace mozilla {

namespace {

StaticMutex gRemoteLazyThreadMutex;
StaticRefPtr<RemoteLazyInputStreamThread> gRemoteLazyThread
    MOZ_GUARDED_BY(gRemoteLazyThreadMutex);

}  // namespace

NS_IMPL_ISUPPORTS(RemoteLazyInputStreamThread, nsIEventTarget,
                  nsISerialEventTarget, nsIDirectTaskDispatcher)

/* static */
already_AddRefed<RemoteLazyInputStreamThread>
RemoteLazyInputStreamThread::Get() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  return do_AddRef(gRemoteLazyThread);
}

/* static */
already_AddRefed<RemoteLazyInputStreamThread>
RemoteLazyInputStreamThread::GetOrCreate() {
  StaticMutexAutoLock lock(gRemoteLazyThreadMutex);

  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return nullptr;
  }

  if (!gRemoteLazyThread) {
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewNamedThread("RemoteLzyStream", getter_AddRefs(thread));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    gRemoteLazyThread =
        new RemoteLazyInputStreamThread(WrapMovingNotNull(thread));

    // Dispatch to the main thread, which will set up a listener
    // to shut down the thread during XPCOMShutdownThreads.
    //
    // We do this even if we're already on the main thread, as
    // if we're too late in shutdown, this will trigger the thread
    // to shut down synchronously.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "RemoteLazyInputStreamThread::MainThreadInit", [] {
          RunOnShutdown(
              [] {
                RefPtr<RemoteLazyInputStreamThread> rlis =
                    RemoteLazyInputStreamThread::Get();
                // This is the only place supposed to ever null our reference.
                MOZ_ASSERT(rlis);
                rlis->mThread->Shutdown();

                StaticMutexAutoLock lock(gRemoteLazyThreadMutex);
                gRemoteLazyThread = nullptr;
              },
              ShutdownPhase::XPCOMShutdownThreads);
        }));
  }

  return do_AddRef(gRemoteLazyThread);
}

// nsIEventTarget

NS_IMETHODIMP_(bool)
RemoteLazyInputStreamThread::IsOnCurrentThreadInfallible() {
  return mThread->IsOnCurrentThread();
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::IsOnCurrentThread(bool* aRetval) {
  return mThread->IsOnCurrentThread(aRetval);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                      uint32_t aFlags) {
  return mThread->Dispatch(std::move(aRunnable), aFlags);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DispatchFromScript(nsIRunnable* aRunnable,
                                                uint32_t aFlags) {
  return mThread->Dispatch(do_AddRef(aRunnable), aFlags);
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DelayedDispatch(already_AddRefed<nsIRunnable>,
                                             uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::RegisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::UnregisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteLazyInputStreamThread::DispatchDirectTask(
    already_AddRefed<nsIRunnable> aRunnable) {
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->DispatchDirectTask(runnable.forget());
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP RemoteLazyInputStreamThread::DrainDirectTasks() {
  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->DrainDirectTasks();
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP RemoteLazyInputStreamThread::HaveDirectTasks(bool* aValue) {
  nsCOMPtr<nsIDirectTaskDispatcher> dispatcher = do_QueryInterface(mThread);

  if (dispatcher) {
    return dispatcher->HaveDirectTasks(aValue);
  }

  return NS_ERROR_FAILURE;
}

bool IsOnDOMFileThread() {
  RefPtr<RemoteLazyInputStreamThread> rlis = RemoteLazyInputStreamThread::Get();
  return rlis && rlis->IsOnCurrentThread();
}

void AssertIsOnDOMFileThread() { MOZ_ASSERT(IsOnDOMFileThread()); }

}  // namespace mozilla
