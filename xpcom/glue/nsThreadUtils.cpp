/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#ifdef MOZILLA_INTERNAL_API
# include "nsThreadManager.h"
#else
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#include "mozilla/WindowsVersion.h"
using mozilla::IsVistaOrLater;
#elif defined(XP_MACOSX)
#include <sys/resource.h>
#endif

#include <pratom.h>
#include <prthread.h>

#ifndef XPCOM_GLUE_AVOID_NSPR

NS_IMPL_ISUPPORTS(nsRunnable, nsIRunnable)

NS_IMETHODIMP
nsRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCancelableRunnable, nsICancelableRunnable,
                  nsIRunnable)

NS_IMETHODIMP
nsCancelableRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

NS_IMETHODIMP
nsCancelableRunnable::Cancel()
{
  // Do nothing
  return NS_OK;
}

#endif  // XPCOM_GLUE_AVOID_NSPR

//-----------------------------------------------------------------------------

NS_METHOD
NS_NewThread(nsIThread** aResult, nsIRunnable* aEvent, uint32_t aStackSize)
{
  nsCOMPtr<nsIThread> thread;
#ifdef MOZILLA_INTERNAL_API
  nsresult rv =
    nsThreadManager::get()->nsThreadManager::NewThread(0, aStackSize,
                                                       getter_AddRefs(thread));
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mgr->NewThread(0, aStackSize, getter_AddRefs(thread));
#endif
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aEvent) {
    rv = thread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  *aResult = nullptr;
  thread.swap(*aResult);
  return NS_OK;
}

NS_METHOD
NS_GetCurrentThread(nsIThread** aResult)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetCurrentThread(aResult);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mgr->GetCurrentThread(aResult);
#endif
}

NS_METHOD
NS_GetMainThread(nsIThread** aResult)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetMainThread(aResult);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return mgr->GetMainThread(aResult);
#endif
}

#if defined(MOZILLA_INTERNAL_API) && defined(XP_WIN)
extern DWORD gTLSThreadIDIndex;
bool
NS_IsMainThread()
{
  return TlsGetValue(gTLSThreadIDIndex) == (void*)mozilla::threads::Main;
}
#elif defined(MOZILLA_INTERNAL_API) && defined(NS_TLS)
#ifdef MOZ_ASAN
// Temporary workaround, see bug 895845
bool
NS_IsMainThread()
{
  return gTLSThreadID == mozilla::threads::Main;
}
#else
// NS_IsMainThread() is defined inline in MainThreadUtils.h
#endif
#else
#ifdef MOZILLA_INTERNAL_API
bool
NS_IsMainThread()
{
  bool result = false;
  nsThreadManager::get()->nsThreadManager::GetIsMainThread(&result);
  return bool(result);
}
#else
bool
NS_IsMainThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr) {
    mgr->GetIsMainThread(&result);
  }
  return bool(result);
}
#endif
#endif

NS_METHOD
NS_DispatchToCurrentThread(nsIRunnable* aEvent)
{
#ifdef MOZILLA_INTERNAL_API
  nsIThread* thread = NS_GetCurrentThread();
  if (!thread) {
    return NS_ERROR_UNEXPECTED;
  }
#else
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif
  return thread->Dispatch(aEvent, NS_DISPATCH_NORMAL);
}

NS_METHOD
NS_DispatchToMainThread(nsIRunnable* aEvent, uint32_t aDispatchFlags)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(thread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return thread->Dispatch(aEvent, aDispatchFlags);
}

#ifndef XPCOM_GLUE_AVOID_NSPR
NS_METHOD
NS_ProcessPendingEvents(nsIThread* aThread, PRIntervalTime aTimeout)
{
  nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
  if (!aThread) {
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return NS_ERROR_UNEXPECTED;
    }
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!aThread) {
    rv = NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    aThread = current.get();
  }
#endif

  PRIntervalTime start = PR_IntervalNow();
  for (;;) {
    bool processedEvent;
    rv = aThread->ProcessNextEvent(false, &processedEvent);
    if (NS_FAILED(rv) || !processedEvent) {
      break;
    }
    if (PR_IntervalNow() - start > aTimeout) {
      break;
    }
  }
  return rv;
}
#endif // XPCOM_GLUE_AVOID_NSPR

inline bool
hasPendingEvents(nsIThread* aThread)
{
  bool val;
  return NS_SUCCEEDED(aThread->HasPendingEvents(&val)) && val;
}

bool
NS_HasPendingEvents(nsIThread* aThread)
{
  if (!aThread) {
#ifndef MOZILLA_INTERNAL_API
    nsCOMPtr<nsIThread> current;
    NS_GetCurrentThread(getter_AddRefs(current));
    return hasPendingEvents(current);
#else
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return false;
    }
#endif
  }
  return hasPendingEvents(aThread);
}

bool
NS_ProcessNextEvent(nsIThread* aThread, bool aMayWait)
{
#ifdef MOZILLA_INTERNAL_API
  if (!aThread) {
    aThread = NS_GetCurrentThread();
    if (NS_WARN_IF(!aThread)) {
      return false;
    }
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!aThread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    if (NS_WARN_IF(!current)) {
      return false;
    }
    aThread = current.get();
  }
#endif
  bool val;
  return NS_SUCCEEDED(aThread->ProcessNextEvent(aMayWait, &val)) && val;
}

#ifndef XPCOM_GLUE_AVOID_NSPR

namespace {

class nsNameThreadRunnable MOZ_FINAL : public nsIRunnable
{
public:
  nsNameThreadRunnable(const nsACString& aName) : mName(aName) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

protected:
  const nsCString mName;
};

NS_IMPL_ISUPPORTS(nsNameThreadRunnable, nsIRunnable)

NS_IMETHODIMP
nsNameThreadRunnable::Run()
{
  PR_SetCurrentThreadName(mName.BeginReading());
  return NS_OK;
}

} // anonymous namespace

void
NS_SetThreadName(nsIThread* aThread, const nsACString& aName)
{
  if (!aThread) {
    return;
  }

  aThread->Dispatch(new nsNameThreadRunnable(aName),
                    nsIEventTarget::DISPATCH_NORMAL);
}

#else // !XPCOM_GLUE_AVOID_NSPR

void
NS_SetThreadName(nsIThread* aThread, const nsACString& aName)
{
  // No NSPR, no love.
}

#endif

#ifdef MOZILLA_INTERNAL_API
nsIThread*
NS_GetCurrentThread()
{
  return nsThreadManager::get()->GetCurrentThread();
}
#endif

// nsThreadPoolNaming
void
nsThreadPoolNaming::SetThreadPoolName(const nsACString& aPoolName,
                                      nsIThread* aThread)
{
  nsCString name(aPoolName);
  name.AppendLiteral(" #");
  name.AppendInt(++mCounter, 10); // The counter is declared as volatile

  if (aThread) {
    // Set on the target thread
    NS_SetThreadName(aThread, name);
  } else {
    // Set on the current thread
    PR_SetCurrentThreadName(name.BeginReading());
  }
}

// nsAutoLowPriorityIO
nsAutoLowPriorityIO::nsAutoLowPriorityIO()
{
#if defined(XP_WIN)
  lowIOPrioritySet = IsVistaOrLater() &&
                     SetThreadPriority(GetCurrentThread(),
                                       THREAD_MODE_BACKGROUND_BEGIN);
#elif defined(XP_MACOSX)
  oldPriority = getiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD);
  lowIOPrioritySet = oldPriority != -1 &&
                     setiopolicy_np(IOPOL_TYPE_DISK,
                                    IOPOL_SCOPE_THREAD,
                                    IOPOL_THROTTLE) != -1;
#else
  lowIOPrioritySet = false;
#endif
}

nsAutoLowPriorityIO::~nsAutoLowPriorityIO()
{
#if defined(XP_WIN)
  if (MOZ_LIKELY(lowIOPrioritySet)) {
    // On Windows the old thread priority is automatically restored
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
  }
#elif defined(XP_MACOSX)
  if (MOZ_LIKELY(lowIOPrioritySet)) {
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, oldPriority);
  }
#endif
}

