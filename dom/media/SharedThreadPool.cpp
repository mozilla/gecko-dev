/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedThreadPool.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "nsDataHashtable.h"
#include "VideoUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#ifdef XP_WIN
#include "ThreadPoolCOMListener.h"
#endif

namespace mozilla {

// Created and destroyed on the main thread.
static StaticAutoPtr<ReentrantMonitor> sMonitor;

// Hashtable, maps thread pool name to SharedThreadPool instance.
// Modified only on the main thread.
static StaticAutoPtr<nsDataHashtable<nsCStringHashKey, SharedThreadPool*>> sPools;

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName);

static void
DestroySharedThreadPoolHashTable();

void
SharedThreadPool::EnsureInitialized()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sMonitor || sPools) {
    // Already initalized.
    return;
  }
  sMonitor = new ReentrantMonitor("SharedThreadPool");
  sPools = new nsDataHashtable<nsCStringHashKey, SharedThreadPool*>();
}

class ShutdownPoolsEvent : public nsRunnable {
public:
  NS_IMETHODIMP Run() {
    MOZ_ASSERT(NS_IsMainThread());
    DestroySharedThreadPoolHashTable();
    return NS_OK;
  }
};

static void
DestroySharedThreadPoolHashTable()
{
  // We have to guard against a lot of funny business here - see the comments
  // below. It would make more sense to just manually initialize/destroy the
  // hashtable on startup/shutdown.
  MOZ_ASSERT(NS_IsMainThread());

  // Check that the pool still exists to guard against this scenario:
  // (1) sPools becomes empty, and we dispatch ShutdownPoolsEvent.
  // (2) A new call to ::Get occurs and sPools becomes non-empty.
  // (3) The new pool is immediately released, causing us to dispatch a second
  //     ShutdownPoolsEvent.
  // (4) The first ShutdownPoolsEvent runs, and deletes sPools.
  // (5) The second ShutdownPoolsEvent runs.
  if (!sPools) {
    MOZ_ASSERT(!sMonitor);
    return;
  }

  // Check that the pool is still empty to guard against this scenario:
  // (1) sPools becomes empty, and we dispatch ShutdownPoolsEvent
  // (2) A new call to ::Get occurs and sPools becomes non-empty.
  // (3) The ShutdownPoolsEvent runs, with sPools now non-empty.
  if (!sPools->Count()) {
    // No more SharedThreadPool singletons. Delete the hash table.
    // Note we don't need to lock sMonitor, since we only modify the
    // hash table on the main thread, and if the hash table is empty
    // there are no external references into its contents.
    sPools = nullptr;
    sMonitor = nullptr;
  }
}

/* static */
void
SharedThreadPool::SpinUntilShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Wait until the ShutdownPoolsEvent has been run and shutdown the pool.
  while (sPools) {
    if (!NS_ProcessNextEvent(NS_GetCurrentThread(), true)) {
      break;
    }
  }
  MOZ_ASSERT(!sPools);
  MOZ_ASSERT(!sMonitor);
}

TemporaryRef<SharedThreadPool>
SharedThreadPool::Get(const nsCString& aName, uint32_t aThreadLimit)
{
  MOZ_ASSERT(NS_IsMainThread());
  EnsureInitialized();
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  SharedThreadPool* pool = nullptr;
  nsresult rv;
  if (!sPools->Get(aName, &pool)) {
    nsCOMPtr<nsIThreadPool> threadPool(CreateThreadPool(aName));
    NS_ENSURE_TRUE(threadPool, nullptr);
    pool = new SharedThreadPool(aName, threadPool);

    // Set the thread and idle limits. Note that we don't rely on the
    // EnsureThreadLimitIsAtLeast() call below, as the default thread limit
    // is 4, and if aThreadLimit is less than 4 we'll end up with a pool
    // with 4 threads rather than what we expected; so we'll have unexpected
    // behaviour.
    rv = pool->SetThreadLimit(aThreadLimit);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = pool->SetIdleThreadLimit(aThreadLimit);
    NS_ENSURE_SUCCESS(rv, nullptr);

    sPools->Put(aName, pool);
  } else if (NS_FAILED(pool->EnsureThreadLimitIsAtLeast(aThreadLimit))) {
    NS_WARNING("Failed to set limits on thread pool");
  }

  MOZ_ASSERT(pool);
  RefPtr<SharedThreadPool> instance(pool);
  return instance.forget();
}

NS_IMETHODIMP_(MozExternalRefCountType) SharedThreadPool::AddRef(void)
{
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;
  NS_LOG_ADDREF(this, count, "SharedThreadPool", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType) SharedThreadPool::Release(void)
{
  MOZ_ASSERT(sMonitor);
  bool dispatchShutdownEvent;
  {
    ReentrantMonitorAutoEnter mon(*sMonitor);
    nsrefcnt count = --mRefCnt;
    NS_LOG_RELEASE(this, count, "SharedThreadPool");
    if (count) {
      return count;
    }

    // Zero refcount. Must shutdown and then delete the thread pool.

    // First, dispatch an event to the main thread to call Shutdown() on
    // the nsIThreadPool. The Runnable here  will add a refcount to the pool,
    // and when the Runnable releases the nsIThreadPool it will be deleted.
    RefPtr<nsIRunnable> r = NS_NewRunnableMethod(mPool, &nsIThreadPool::Shutdown);
    NS_DispatchToMainThread(r);

    // Remove SharedThreadPool from table of pools.
    sPools->Remove(mName);
    MOZ_ASSERT(!sPools->Get(mName));

    // Stabilize refcount, so that if something in the dtor QIs,
    // it won't explode.
    mRefCnt = 1;

    delete this;

    dispatchShutdownEvent = sPools->Count() == 0;
  }
  if (dispatchShutdownEvent) {
    // No more SharedThreadPools alive. Destroy the hash table.
    // Ensure that we only run on the main thread.
    // Do this in an event so that if something holds the monitor we won't
    // be deleting the monitor while it's held.
    NS_DispatchToMainThread(new ShutdownPoolsEvent());
  }
  return 0;
}

NS_IMPL_QUERY_INTERFACE(SharedThreadPool, nsIThreadPool, nsIEventTarget)

SharedThreadPool::SharedThreadPool(const nsCString& aName,
                                   nsIThreadPool* aPool)
  : mName(aName)
  , mPool(aPool)
  , mRefCnt(0)
{
  MOZ_COUNT_CTOR(SharedThreadPool);
  mEventTarget = do_QueryInterface(aPool);
}

SharedThreadPool::~SharedThreadPool()
{
  MOZ_COUNT_DTOR(SharedThreadPool);
}

nsresult
SharedThreadPool::EnsureThreadLimitIsAtLeast(uint32_t aLimit)
{
  // We limit the number of threads that we use for media. Note that we
  // set the thread limit to the same as the idle limit so that we're not
  // constantly creating and destroying threads (see Bug 881954). When the
  // thread pool threads shutdown they dispatch an event to the main thread
  // to call nsIThread::Shutdown(), and if we're very busy that can take a
  // while to run, and we end up with dozens of extra threads. Note that
  // threads that are idle for 60 seconds are shutdown naturally.
  uint32_t existingLimit = 0;
  nsresult rv;

  rv = mPool->GetThreadLimit(&existingLimit);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aLimit > existingLimit) {
    rv = mPool->SetThreadLimit(aLimit);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mPool->GetIdleThreadLimit(&existingLimit);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aLimit > existingLimit) {
    rv = mPool->SetIdleThreadLimit(aLimit);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetName(aName);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetThreadStackSize(MEDIA_THREAD_STACK_SIZE);
  NS_ENSURE_SUCCESS(rv, nullptr);

#ifdef XP_WIN
  // Ensure MSCOM is initialized on the thread pools threads.
  nsCOMPtr<nsIThreadPoolListener> listener = new MSCOMInitThreadPoolListener();
  rv = pool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, nullptr);
#endif

  return pool.forget();
}

} // namespace mozilla
