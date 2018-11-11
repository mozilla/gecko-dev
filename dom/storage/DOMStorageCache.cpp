/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorageCache.h"

#include "DOMStorage.h"
#include "DOMStorageDBThread.h"
#include "DOMStorageIPC.h"
#include "DOMStorageManager.h"

#include "nsAutoPtr.h"
#include "nsDOMString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

#define DOM_STORAGE_CACHE_KEEP_ALIVE_TIME_MS 20000

// static
DOMStorageDBBridge* DOMStorageCache::sDatabase = nullptr;
bool DOMStorageCache::sDatabaseDown = false;

namespace {

const uint32_t kDefaultSet = 0;
const uint32_t kPrivateSet = 1;
const uint32_t kSessionSet = 2;

inline uint32_t
GetDataSetIndex(bool aPrivate, bool aSessionOnly)
{
  if (aPrivate) {
    return kPrivateSet;
  }

  if (aSessionOnly) {
    return kSessionSet;
  }

  return kDefaultSet;
}

inline uint32_t
GetDataSetIndex(const DOMStorage* aStorage)
{
  return GetDataSetIndex(aStorage->IsPrivate(), aStorage->IsSessionOnly());
}

} // namespace

// DOMStorageCacheBridge

NS_IMPL_ADDREF(DOMStorageCacheBridge)

// Since there is no consumer of return value of Release, we can turn this 
// method to void to make implementation of asynchronous DOMStorageCache::Release
// much simpler.
NS_IMETHODIMP_(void) DOMStorageCacheBridge::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "DOMStorageCacheBridge");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(_class); */
    delete (this);
  }
}

// DOMStorageCache

DOMStorageCache::DOMStorageCache(const nsACString* aOriginNoSuffix)
: mOriginNoSuffix(*aOriginNoSuffix)
, mMonitor("DOMStorageCache")
, mLoaded(false)
, mLoadResult(NS_OK)
, mInitialized(false)
, mPersistent(false)
, mSessionOnlyDataSetActive(false)
, mPreloadTelemetryRecorded(false)
{
  MOZ_COUNT_CTOR(DOMStorageCache);
}

DOMStorageCache::~DOMStorageCache()
{
  if (mManager) {
    mManager->DropCache(this);
  }

  MOZ_COUNT_DTOR(DOMStorageCache);
}

NS_IMETHODIMP_(void)
DOMStorageCache::Release(void)
{
  // We must actually release on the main thread since the cache removes it
  // self from the manager's hash table.  And we don't want to lock access to
  // that hash table.
  if (NS_IsMainThread()) {
    DOMStorageCacheBridge::Release();
    return;
  }

  RefPtr<nsRunnableMethod<DOMStorageCacheBridge, void, false> > event =
    NewNonOwningRunnableMethod(static_cast<DOMStorageCacheBridge*>(this),
                               &DOMStorageCacheBridge::Release);

  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("DOMStorageCache::Release() on a non-main thread");
    DOMStorageCacheBridge::Release();
  }
}

void
DOMStorageCache::Init(DOMStorageManager* aManager,
                      bool aPersistent,
                      nsIPrincipal* aPrincipal,
                      const nsACString& aQuotaOriginScope)
{
  if (mInitialized) {
    return;
  }

  mInitialized = true;
  mPrincipal = aPrincipal;
  BasePrincipal::Cast(aPrincipal)->OriginAttributesRef().CreateSuffix(mOriginSuffix);
  mPersistent = aPersistent;
  if (aQuotaOriginScope.IsEmpty()) {
    mQuotaOriginScope = Origin();
  } else {
    mQuotaOriginScope = aQuotaOriginScope;
  }

  if (mPersistent) {
    mManager = aManager;
    Preload();
  }

  // Check the quota string has (or has not) the identical origin suffix as
  // this storage cache is bound to.
  MOZ_ASSERT(StringBeginsWith(mQuotaOriginScope, mOriginSuffix));
  MOZ_ASSERT(mOriginSuffix.IsEmpty() != StringBeginsWith(mQuotaOriginScope, 
                                                         NS_LITERAL_CSTRING("^")));

  mUsage = aManager->GetOriginUsage(mQuotaOriginScope);
}

inline bool
DOMStorageCache::Persist(const DOMStorage* aStorage) const
{
  return mPersistent &&
         !aStorage->IsSessionOnly() &&
         !aStorage->IsPrivate();
}

const nsCString
DOMStorageCache::Origin() const
{
  return DOMStorageManager::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

DOMStorageCache::Data&
DOMStorageCache::DataSet(const DOMStorage* aStorage)
{
  uint32_t index = GetDataSetIndex(aStorage);

  if (index == kSessionSet && !mSessionOnlyDataSetActive) {
    // Session only data set is demanded but not filled with
    // current data set, copy to session only set now.

    WaitForPreload(Telemetry::LOCALDOMSTORAGE_SESSIONONLY_PRELOAD_BLOCKING_MS);

    Data& defaultSet = mData[kDefaultSet];
    Data& sessionSet = mData[kSessionSet];

    for (auto iter = defaultSet.mKeys.Iter(); !iter.Done(); iter.Next()) {
      sessionSet.mKeys.Put(iter.Key(), iter.UserData());
    }

    mSessionOnlyDataSetActive = true;

    // This updates sessionSet.mOriginQuotaUsage and also updates global usage
    // for all session only data
    ProcessUsageDelta(kSessionSet, defaultSet.mOriginQuotaUsage);
  }

  return mData[index];
}

bool
DOMStorageCache::ProcessUsageDelta(const DOMStorage* aStorage, int64_t aDelta)
{
  return ProcessUsageDelta(GetDataSetIndex(aStorage), aDelta);
}

bool
DOMStorageCache::ProcessUsageDelta(uint32_t aGetDataSetIndex, const int64_t aDelta)
{
  // Check if we are in a low disk space situation
  if (aDelta > 0 && mManager && mManager->IsLowDiskSpace()) {
    return false;
  }

  // Check limit per this origin
  Data& data = mData[aGetDataSetIndex];
  uint64_t newOriginUsage = data.mOriginQuotaUsage + aDelta;
  if (aDelta > 0 && newOriginUsage > DOMStorageManager::GetQuota()) {
    return false;
  }

  // Now check eTLD+1 limit
  if (mUsage && !mUsage->CheckAndSetETLD1UsageDelta(aGetDataSetIndex, aDelta)) {
    return false;
  }

  // Update size in our data set
  data.mOriginQuotaUsage = newOriginUsage;
  return true;
}

void
DOMStorageCache::Preload()
{
  if (mLoaded || !mPersistent) {
    return;
  }

  if (!StartDatabase()) {
    mLoaded = true;
    mLoadResult = NS_ERROR_FAILURE;
    return;
  }

  sDatabase->AsyncPreload(this);
}

namespace {

// This class is passed to timer as a tick observer.  It refers the cache
// and keeps it alive for a time.
class DOMStorageCacheHolder : public nsITimerCallback
{
  virtual ~DOMStorageCacheHolder() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    mCache = nullptr;
    return NS_OK;
  }

  RefPtr<DOMStorageCache> mCache;

public:
  explicit DOMStorageCacheHolder(DOMStorageCache* aCache) : mCache(aCache) {}
};

NS_IMPL_ISUPPORTS(DOMStorageCacheHolder, nsITimerCallback)

} // namespace

void
DOMStorageCache::KeepAlive()
{
  // Missing reference back to the manager means the cache is not responsible
  // for its lifetime.  Used for keeping sessionStorage live forever.
  if (!mManager) {
    return;
  }

  if (!NS_IsMainThread()) {
    // Timer and the holder must be initialized on the main thread.
    NS_DispatchToMainThread(NewRunnableMethod(this, &DOMStorageCache::KeepAlive));
    return;
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  if (!timer) {
    return;
  }

  RefPtr<DOMStorageCacheHolder> holder = new DOMStorageCacheHolder(this);
  timer->InitWithCallback(holder, DOM_STORAGE_CACHE_KEEP_ALIVE_TIME_MS,
                          nsITimer::TYPE_ONE_SHOT);

  mKeepAliveTimer.swap(timer);
}

namespace {

// The AutoTimer provided by telemetry headers is only using static,
// i.e. compile time known ID, but here we know the ID only at run time.
// Hence a new class.
class TelemetryAutoTimer
{
public:
  explicit TelemetryAutoTimer(Telemetry::ID aId)
    : id(aId), start(TimeStamp::Now()) {}
  ~TelemetryAutoTimer()
    { Telemetry::AccumulateDelta_impl<Telemetry::Millisecond>::compute(id, start); }
private:
  Telemetry::ID id;
  const TimeStamp start;
};

} // namespace

void
DOMStorageCache::WaitForPreload(Telemetry::ID aTelemetryID)
{
  if (!mPersistent) {
    return;
  }

  bool loaded = mLoaded;

  // Telemetry of rates of pending preloads
  if (!mPreloadTelemetryRecorded) {
    mPreloadTelemetryRecorded = true;
    Telemetry::Accumulate(
      Telemetry::LOCALDOMSTORAGE_PRELOAD_PENDING_ON_FIRST_ACCESS,
      !loaded);
  }

  if (loaded) {
    return;
  }

  // Measure which operation blocks and for how long
  TelemetryAutoTimer timer(aTelemetryID);

  // If preload already started (i.e. we got some first data, but not all)
  // SyncPreload will just wait for it to finish rather then synchronously
  // read from the database.  It seems to me more optimal.

  // TODO place for A/B testing (force main thread load vs. let preload finish)

  // No need to check sDatabase for being non-null since preload is either
  // done before we've shut the DB down or when the DB could not start,
  // preload has not even be started.
  sDatabase->SyncPreload(this);
}

nsresult
DOMStorageCache::GetLength(const DOMStorage* aStorage, uint32_t* aRetval)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETLENGTH_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  *aRetval = DataSet(aStorage).mKeys.Count();
  return NS_OK;
}

nsresult
DOMStorageCache::GetKey(const DOMStorage* aStorage, uint32_t aIndex, nsAString& aRetval)
{
  // XXX: This does a linear search for the key at index, which would
  // suck if there's a large numer of indexes. Do we care? If so,
  // maybe we need to have a lazily populated key array here or
  // something?
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  aRetval.SetIsVoid(true);
  for (auto iter = DataSet(aStorage).mKeys.Iter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aRetval = iter.Key();
      break;
    }
    aIndex--;
  }

  return NS_OK;
}

void
DOMStorageCache::GetKeys(const DOMStorage* aStorage, nsTArray<nsString>& aKeys)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETALLKEYS_BLOCKING_MS);
  }

  if (NS_FAILED(mLoadResult)) {
    return;
  }

  for (auto iter = DataSet(aStorage).mKeys.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

nsresult
DOMStorageCache::GetItem(const DOMStorage* aStorage, const nsAString& aKey,
                         nsAString& aRetval)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!DataSet(aStorage).mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }

  aRetval = value;

  return NS_OK;
}

nsresult
DOMStorageCache::SetItem(const DOMStorage* aStorage, const nsAString& aKey,
                         const nsString& aValue, nsString& aOld)
{
  // Size of the cache that will change after this action.
  int64_t delta = 0;

  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_SETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);

    // We only consider key size if the key doesn't exist before.
    delta += static_cast<int64_t>(aKey.Length());
  }

  delta += static_cast<int64_t>(aValue.Length()) -
           static_cast<int64_t>(aOld.Length());

  if (!ProcessUsageDelta(aStorage, delta)) {
    return NS_ERROR_DOM_QUOTA_REACHED;
  }

  if (aValue == aOld && DOMStringIsNull(aValue) == DOMStringIsNull(aOld)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  data.mKeys.Put(aKey, aValue);

  if (Persist(aStorage)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    if (DOMStringIsNull(aOld)) {
      return sDatabase->AsyncAddItem(this, aKey, aValue);
    }

    return sDatabase->AsyncUpdateItem(this, aKey, aValue);
  }

  return NS_OK;
}

nsresult
DOMStorageCache::RemoveItem(const DOMStorage* aStorage, const nsAString& aKey,
                            nsString& aOld)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_REMOVEKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  const int64_t delta = -(static_cast<int64_t>(aOld.Length()) +
                          static_cast<int64_t>(aKey.Length()));
  Unused << ProcessUsageDelta(aStorage, delta);
  data.mKeys.Remove(aKey);

  if (Persist(aStorage)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return sDatabase->AsyncRemoveItem(this, aKey);
  }

  return NS_OK;
}

nsresult
DOMStorageCache::Clear(const DOMStorage* aStorage)
{
  bool refresh = false;
  if (Persist(aStorage)) {
    // We need to preload all data (know the size) before we can proceeed
    // to correctly decrease cached usage number.
    // XXX as in case of unload, this is not technically needed now, but
    // after super-scope quota introduction we have to do this.  Get telemetry
    // right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_CLEAR_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      // When we failed to load data from the database, force delete of the
      // scope data and make use of the storage possible again.
      refresh = true;
      mLoadResult = NS_OK;
    }
  }

  Data& data = DataSet(aStorage);
  bool hadData = !!data.mKeys.Count();

  if (hadData) {
    Unused << ProcessUsageDelta(aStorage, -data.mOriginQuotaUsage);
    data.mKeys.Clear();
  }

  if (Persist(aStorage) && (refresh || hadData)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return sDatabase->AsyncClear(this);
  }

  return hadData ? NS_OK : NS_SUCCESS_DOM_NO_OPERATION;
}

void
DOMStorageCache::CloneFrom(const DOMStorageCache* aThat)
{
  // This will never be called on anything else than SessionStorage.
  // This means mData will never be touched on any other thread than
  // the main thread and it never went through the loading process.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPersistent);
  MOZ_ASSERT(!(bool)aThat->mLoaded);

  mLoaded = false;
  mInitialized = aThat->mInitialized;
  mPersistent = false;
  mSessionOnlyDataSetActive = aThat->mSessionOnlyDataSetActive;

  for (uint32_t i = 0; i < kDataSetCount; ++i) {
    for (auto it = aThat->mData[i].mKeys.ConstIter(); !it.Done(); it.Next()) {
      mData[i].mKeys.Put(it.Key(), it.UserData());
    }
    ProcessUsageDelta(i, aThat->mData[i].mOriginQuotaUsage);
  }
}

// Defined in DOMStorageManager.cpp
extern bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal, nsIPrincipal* aSubjectPrincipal);

bool
DOMStorageCache::CheckPrincipal(nsIPrincipal* aPrincipal) const
{
  return PrincipalsEqual(mPrincipal, aPrincipal);
}

void
DOMStorageCache::UnloadItems(uint32_t aUnloadFlags)
{
  if (aUnloadFlags & kUnloadDefault) {
    // Must wait for preload to pass correct usage to ProcessUsageDelta
    // XXX this is not technically needed right now since there is just
    // per-origin isolated quota handling, but when we introduce super-
    // -scope quotas, we have to do this.  Better to start getting
    // telemetry right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    ProcessUsageDelta(kDefaultSet, -mData[kDefaultSet].mOriginQuotaUsage);
  }

  if (aUnloadFlags & kUnloadPrivate) {
    mData[kPrivateSet].mKeys.Clear();
    ProcessUsageDelta(kPrivateSet, -mData[kPrivateSet].mOriginQuotaUsage);
  }

  if (aUnloadFlags & kUnloadSession) {
    mData[kSessionSet].mKeys.Clear();
    ProcessUsageDelta(kSessionSet, -mData[kSessionSet].mOriginQuotaUsage);
    mSessionOnlyDataSetActive = false;
  }

#ifdef DOM_STORAGE_TESTS
  if (aUnloadFlags & kTestReload) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    mLoaded = false; // This is only used in testing code
    Preload();
  }
#endif
}

// DOMStorageCacheBridge

uint32_t
DOMStorageCache::LoadedCount()
{
  MonitorAutoLock monitor(mMonitor);
  Data& data = mData[kDefaultSet];
  return data.mKeys.Count();
}

bool
DOMStorageCache::LoadItem(const nsAString& aKey, const nsString& aValue)
{
  MonitorAutoLock monitor(mMonitor);
  if (mLoaded) {
    return false;
  }

  Data& data = mData[kDefaultSet];
  if (data.mKeys.Get(aKey, nullptr)) {
    return true; // don't stop, just don't override
  }

  data.mKeys.Put(aKey, aValue);
  data.mOriginQuotaUsage += aKey.Length() + aValue.Length();
  return true;
}

void
DOMStorageCache::LoadDone(nsresult aRv)
{
  // Keep the preloaded cache alive for a time
  KeepAlive();

  MonitorAutoLock monitor(mMonitor);
  mLoadResult = aRv;
  mLoaded = true;
  monitor.Notify();
}

void
DOMStorageCache::LoadWait()
{
  MonitorAutoLock monitor(mMonitor);
  while (!mLoaded) {
    monitor.Wait();
  }
}

// DOMStorageUsage

DOMStorageUsage::DOMStorageUsage(const nsACString& aOriginScope)
  : mOriginScope(aOriginScope)
{
  mUsage[kDefaultSet] = mUsage[kPrivateSet] = mUsage[kSessionSet] = 0LL;
}

namespace {

class LoadUsageRunnable : public Runnable
{
public:
  LoadUsageRunnable(int64_t* aUsage, const int64_t aDelta)
    : mTarget(aUsage)
    , mDelta(aDelta)
  {}

private:
  int64_t* mTarget;
  int64_t mDelta;

  NS_IMETHOD Run() override { *mTarget = mDelta; return NS_OK; }
};

} // namespace

void
DOMStorageUsage::LoadUsage(const int64_t aUsage)
{
  // Using kDefaultSet index since it is the index for the persitent data
  // stored in the database we have just loaded usage for.
  if (!NS_IsMainThread()) {
    // In single process scenario we get this call from the DB thread
    RefPtr<LoadUsageRunnable> r =
      new LoadUsageRunnable(mUsage + kDefaultSet, aUsage);
    NS_DispatchToMainThread(r);
  } else {
    // On a child process we get this on the main thread already
    mUsage[kDefaultSet] += aUsage;
  }
}

bool
DOMStorageUsage::CheckAndSetETLD1UsageDelta(uint32_t aDataSetIndex, const int64_t aDelta)
{
  MOZ_ASSERT(NS_IsMainThread());

  int64_t newUsage = mUsage[aDataSetIndex] + aDelta;
  if (aDelta > 0 && newUsage > DOMStorageManager::GetQuota()) {
    return false;
  }

  mUsage[aDataSetIndex] = newUsage;
  return true;
}


// static
DOMStorageDBBridge*
DOMStorageCache::StartDatabase()
{
  if (sDatabase || sDatabaseDown) {
    // When sDatabaseDown is at true, sDatabase is null.
    // Checking sDatabaseDown flag here prevents reinitialization of
    // the database after shutdown.
    return sDatabase;
  }

  if (XRE_IsParentProcess()) {
    nsAutoPtr<DOMStorageDBThread> db(new DOMStorageDBThread());

    nsresult rv = db->Init();
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    sDatabase = db.forget();
  } else {
    // Use DOMLocalStorageManager::Ensure in case we're called from
    // DOMSessionStorageManager's initializer and we haven't yet initialized the
    // local storage manager.
    RefPtr<DOMStorageDBChild> db = new DOMStorageDBChild(
        DOMLocalStorageManager::Ensure());

    nsresult rv = db->Init();
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    db.forget(&sDatabase);
  }

  return sDatabase;
}

// static
DOMStorageDBBridge*
DOMStorageCache::GetDatabase()
{
  return sDatabase;
}

// static
nsresult
DOMStorageCache::StopDatabase()
{
  if (!sDatabase) {
    return NS_OK;
  }

  sDatabaseDown = true;

  nsresult rv = sDatabase->Shutdown();
  if (XRE_IsParentProcess()) {
    delete sDatabase;
  } else {
    DOMStorageDBChild* child = static_cast<DOMStorageDBChild*>(sDatabase);
    NS_RELEASE(child);
  }

  sDatabase = nullptr;
  return rv;
}

} // namespace dom
} // namespace mozilla
