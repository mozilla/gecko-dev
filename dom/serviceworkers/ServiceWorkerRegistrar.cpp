/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrar.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/glean/DomServiceworkersMetrics.h"
#include "mozilla/StaticPrefs_dom.h"

#include "nsIEventTarget.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsIServiceWorkerManager.h"
#include "nsIURI.h"
#include "nsIWritablePropertyBag2.h"

#include "MainThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "ServiceWorkerUtils.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

namespace {

static const uint32_t gSupportedRegistrarVersions[] = {
    SERVICEWORKERREGISTRAR_VERSION, 8, 7, 6, 5, 4, 3, 2};

static const uint32_t kInvalidGeneration = static_cast<uint32_t>(-1);

StaticRefPtr<ServiceWorkerRegistrar> gServiceWorkerRegistrar;

nsresult GetOriginAndBaseDomain(const nsACString& aURL, nsACString& aOrigin,
                                nsACString& aBaseDomain) {
  nsCOMPtr<nsIURI> url;
  nsresult rv = NS_NewURI(getter_AddRefs(url), aURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(url, attrs);
  if (!principal) {
    return NS_ERROR_NULL_POINTER;
  }

  rv = principal->GetOriginNoSuffix(aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = principal->GetBaseDomain(aBaseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult ReadLine(nsILineInputStream* aStream, nsACString& aValue) {
  bool hasMoreLines;
  nsresult rv = aStream->ReadLine(aValue, &hasMoreLines);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasMoreLines)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult CreatePrincipalInfo(nsILineInputStream* aStream,
                             ServiceWorkerRegistrationData& aEntry,
                             bool aSkipSpec = false) {
  nsAutoCString suffix;
  nsresult rv = ReadLine(aStream, suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(suffix)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aSkipSpec) {
    nsAutoCString unused;
    nsresult rv = ReadLine(aStream, unused);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = ReadLine(aStream, aEntry.scope());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  nsCString baseDomain;
  rv = GetOriginAndBaseDomain(aEntry.scope(), origin, baseDomain);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aEntry.principal() = mozilla::ipc::ContentPrincipalInfo(
      attrs, origin, aEntry.scope(), Nothing(), baseDomain);

  return NS_OK;
}

MOZ_RUNINIT const IPCNavigationPreloadState
    gDefaultNavigationPreloadState(false, "true"_ns);

}  // namespace

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrar, nsIObserver, nsIAsyncShutdownBlocker)

void ServiceWorkerRegistrar::Initialize() {
  MOZ_ASSERT(!gServiceWorkerRegistrar);

  if (!XRE_IsParentProcess()) {
    return;
  }

  gServiceWorkerRegistrar = new ServiceWorkerRegistrar();
  ClearOnShutdown(&gServiceWorkerRegistrar);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    DebugOnly<nsresult> rv = obs->AddObserver(gServiceWorkerRegistrar,
                                              "profile-after-change", false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

/* static */
already_AddRefed<ServiceWorkerRegistrar> ServiceWorkerRegistrar::Get() {
  MOZ_ASSERT(XRE_IsParentProcess());

  MOZ_ASSERT(gServiceWorkerRegistrar);
  RefPtr<ServiceWorkerRegistrar> service = gServiceWorkerRegistrar.get();
  return service.forget();
}

ServiceWorkerRegistrar::ServiceWorkerRegistrar()
    : mMonitor("ServiceWorkerRegistrar.mMonitor"),
      mDataLoaded(false),
      mDataGeneration(kInvalidGeneration),
      mFileGeneration(kInvalidGeneration),
      mRetryCount(0),
      mShuttingDown(false),
      mSaveDataRunnableDispatched(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

ServiceWorkerRegistrar::~ServiceWorkerRegistrar() {
  MOZ_ASSERT(!mSaveDataRunnableDispatched);
}

void ServiceWorkerRegistrar::GetRegistrations(
    nsTArray<ServiceWorkerRegistrationData>& aValues) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aValues.IsEmpty());

  MonitorAutoLock lock(mMonitor);

  // If we don't have the profile directory, profile is not started yet (and
  // probably we are in a utest).
  if (!mProfileDir) {
    return;
  }

  // We care just about the first execution because this can be blocked by
  // loading data from disk.
  static bool firstTime = true;
  TimeStamp startTime;

  if (firstTime) {
    startTime = TimeStamp::NowLoRes();
  }

  // Waiting for data loaded.
  mMonitor.AssertCurrentThreadOwns();
  while (!mDataLoaded) {
    mMonitor.Wait();
  }

  for (const ServiceWorkerData& data : mData) {
    aValues.AppendElement(data.mRegistration);
  }

  MaybeResetGeneration();
  MOZ_DIAGNOSTIC_ASSERT(mDataGeneration != kInvalidGeneration);
  MOZ_DIAGNOSTIC_ASSERT(mFileGeneration != kInvalidGeneration);

  if (firstTime) {
    firstTime = false;
    glean::service_worker::registration_loading.AccumulateRawDuration(
        TimeStamp::Now() - startTime);
  }
}

namespace {

bool Equivalent(const ServiceWorkerRegistrationData& aLeft,
                const ServiceWorkerRegistrationData& aRight) {
  MOZ_ASSERT(aLeft.principal().type() ==
             mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
  MOZ_ASSERT(aRight.principal().type() ==
             mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

  const auto& leftPrincipal = aLeft.principal().get_ContentPrincipalInfo();
  const auto& rightPrincipal = aRight.principal().get_ContentPrincipalInfo();

  // Only compare the attributes, not the spec part of the principal.
  // The scope comparison above already covers the origin and codebase
  // principals include the full path in their spec which is not what
  // we want here.
  return aLeft.scope() == aRight.scope() &&
         leftPrincipal.attrs() == rightPrincipal.attrs();
}

}  // anonymous namespace

void ServiceWorkerRegistrar::RegisterServiceWorker(
    const ServiceWorkerRegistrationData& aData) {
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to register a serviceWorker during shutting down.");
    return;
  }

  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);
    RegisterServiceWorkerInternal(aData);
  }

  MaybeScheduleSaveData();
  StorageActivityService::SendActivity(aData.principal());
}

void ServiceWorkerRegistrar::UnregisterServiceWorker(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope) {
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to unregister a serviceWorker during shutting down.");
    return;
  }

  bool deleted = false;

  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);

    ServiceWorkerRegistrationData tmp;
    tmp.principal() = aPrincipalInfo;
    tmp.scope() = aScope;

    for (uint32_t i = 0; i < mData.Length(); ++i) {
      if (Equivalent(tmp, mData[i].mRegistration)) {
        UnregisterExpandoCallbacks(CopyableTArray<ServiceWorkerData>{mData[i]});

        mData.RemoveElementAt(i);
        mDataGeneration = GetNextGeneration();
        deleted = true;
        break;
      }
    }
  }

  if (deleted) {
    MaybeScheduleSaveData();
    StorageActivityService::SendActivity(aPrincipalInfo);
  }
}

void ServiceWorkerRegistrar::StoreServiceWorkerExpandoOnMainThread(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    const nsACString& aKey, const nsACString& aValue) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aValue.Contains('\n'), "Invalid chars in the value");

  nsCOMPtr<nsISerialEventTarget> backgroundThread =
      BackgroundParent::GetBackgroundThread();
  if (NS_WARN_IF(!backgroundThread)) {
    // Probably we are shutting down. Unfortunately this expando data will not
    // be stored.
    return;
  }

  backgroundThread->Dispatch(NS_NewRunnableFunction(
      __func__,
      [self = RefPtr(this), aPrincipalInfo, aScope = nsCString(aScope),
       aKey = nsCString(aKey), aValue = nsCString(aValue)]() {
        if (self->mShuttingDown) {
          NS_WARNING(
              "Failed to store an expando to a serviceWorker during shutting "
              "down.");
          return;
        }

        const ExpandoHandler* expandoHandler = nullptr;

        for (const ExpandoHandler& handler : self->mExpandoHandlers) {
          if (handler.mKey == aKey) {
            expandoHandler = &handler;
            break;
          }
        }

        if (!expandoHandler) {
          NS_WARNING("Unsupported handler");
          return;
        }

        bool saveNeeded = false;

        {
          MonitorAutoLock lock(self->mMonitor);
          MOZ_ASSERT(self->mDataLoaded);

          ServiceWorkerRegistrationData tmp;
          tmp.principal() = aPrincipalInfo;
          tmp.scope() = aScope;

          for (uint32_t i = 0; i < self->mData.Length(); ++i) {
            if (Equivalent(tmp, self->mData[i].mRegistration)) {
              bool found = false;
              for (ExpandoData& expando : self->mData[i].mExpandos) {
                if (expando.mKey == aKey) {
                  MOZ_ASSERT(expando.mHandler == expandoHandler);
                  expando.mValue = aValue;
                  found = true;
                  break;
                }
              }

              if (!found) {
                self->mData[i].mExpandos.AppendElement(ExpandoData{
                    nsCString(aKey), nsCString(aValue), expandoHandler});
              }

              self->mDataGeneration = self->GetNextGeneration();
              saveNeeded = true;
              break;
            }
          }
        }

        if (saveNeeded) {
          self->MaybeScheduleSaveData();
          StorageActivityService::SendActivity(aPrincipalInfo);
        }
      }));
}

void ServiceWorkerRegistrar::UnstoreServiceWorkerExpandoOnMainThread(
    const PrincipalInfo& aPrincipalInfo, const nsACString& aScope,
    const nsACString& aKey) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsISerialEventTarget> backgroundThread =
      BackgroundParent::GetBackgroundThread();
  if (NS_WARN_IF(!backgroundThread)) {
    // Probably we are shutting down. Unfortunately this expando data will not
    // be stored.
    return;
  }

  backgroundThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr(this), aPrincipalInfo,
                 aScope = nsCString(aScope), aKey = nsCString(aKey)]() {
        if (self->mShuttingDown) {
          NS_WARNING(
              "Failed to unstore an expando from a serviceWorker during "
              "shutting down.");
          return;
        }

        bool saveNeeded = false;

        {
          MonitorAutoLock lock(self->mMonitor);
          MOZ_ASSERT(self->mDataLoaded);

          ServiceWorkerRegistrationData tmp;
          tmp.principal() = aPrincipalInfo;
          tmp.scope() = aScope;

          for (ServiceWorkerData& data : self->mData) {
            if (Equivalent(tmp, data.mRegistration)) {
              for (uint32_t i = 0; i < data.mExpandos.Length(); ++i) {
                if (data.mExpandos[i].mKey == aKey) {
                  data.mExpandos.RemoveElementAt(i);
                  self->mDataGeneration = self->GetNextGeneration();
                  saveNeeded = true;
                  break;
                }
              }

              break;
            }
          }
        }

        if (saveNeeded) {
          self->MaybeScheduleSaveData();
          StorageActivityService::SendActivity(aPrincipalInfo);
        }
      }));
}

void ServiceWorkerRegistrar::RemoveAll() {
  AssertIsOnBackgroundThread();

  if (mShuttingDown) {
    NS_WARNING("Failed to remove all the serviceWorkers during shutting down.");
    return;
  }

  bool deleted = false;

  nsTArray<ServiceWorkerRegistrationData> data;
  nsTArray<ServiceWorkerData> registrationsWithExpandos;
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(mDataLoaded);

    // Let's take a copy in order to inform StorageActivityService.
    for (const ServiceWorkerData& i : mData) {
      data.AppendElement(i.mRegistration);

      if (!i.mExpandos.IsEmpty()) {
        registrationsWithExpandos.AppendElement(i);
      }
    }

    deleted = !mData.IsEmpty();
    mData.Clear();

    mDataGeneration = GetNextGeneration();
  }

  if (!deleted) {
    return;
  }

  if (!registrationsWithExpandos.IsEmpty()) {
    UnregisterExpandoCallbacks(registrationsWithExpandos);
  }

  MaybeScheduleSaveData();

  for (uint32_t i = 0, len = data.Length(); i < len; ++i) {
    StorageActivityService::SendActivity(data[i].principal());
  }
}

void ServiceWorkerRegistrar::LoadData() {
  MOZ_ASSERT(!NS_IsMainThread());
#ifdef DEBUG
  {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(!mDataLoaded);
  }
#endif

  nsresult rv = ReadData();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    DeleteData();
    // Also if the reading failed we have to notify what is waiting for data.
  }

  MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(!mDataLoaded);
  mDataLoaded = true;
  mMonitor.Notify();
}

bool ServiceWorkerRegistrar::ReloadDataForTest() {
  if (NS_WARN_IF(!StaticPrefs::dom_serviceWorkers_testing_enabled())) {
    return false;
  }

  MOZ_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(mMonitor);
  mData.Clear();
  mDataLoaded = false;

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("dom::ServiceWorkerRegistrar::LoadData", this,
                        &ServiceWorkerRegistrar::LoadData);
  nsresult rv = target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
    return false;
  }

  mMonitor.AssertCurrentThreadOwns();
  while (!mDataLoaded) {
    mMonitor.Wait();
  }

  return mDataLoaded;
}

nsresult ServiceWorkerRegistrar::ReadData() {
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);

    if (!mProfileDir) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv = file->Append(nsLiteralString(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(stream);
  MOZ_ASSERT(lineInputStream);

  nsAutoCString versionStr;
  bool hasMoreLines;
  rv = lineInputStream->ReadLine(versionStr, &hasMoreLines);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t version = versionStr.ToUnsignedInteger(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!IsSupportedVersion(version)) {
    nsContentUtils::LogMessageToConsole(
        nsPrintfCString("Unsupported service worker registrar version: %s",
                        versionStr.get())
            .get());
    return NS_ERROR_FAILURE;
  }

  nsTArray<ServiceWorkerData> tmpData;

  bool overwrite = false;
  bool dedupe = false;
  while (hasMoreLines) {
    ServiceWorkerData* entry = tmpData.AppendElement();

#define GET_LINE(x)                                 \
  rv = lineInputStream->ReadLine(x, &hasMoreLines); \
  if (NS_WARN_IF(NS_FAILED(rv))) {                  \
    return rv;                                      \
  }                                                 \
  if (NS_WARN_IF(!hasMoreLines)) {                  \
    return NS_ERROR_FAILURE;                        \
  }

    nsAutoCString line;
    switch (version) {
      case SERVICEWORKERREGISTRAR_VERSION:
        [[fallthrough]];
      case 9: {
        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        nsAutoCString fetchFlag;
        GET_LINE(fetchFlag);
        if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
            !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
          return NS_ERROR_INVALID_ARG;
        }
        entry->mRegistration.currentWorkerHandlesFetch() =
            fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        nsAutoCString updateViaCache;
        GET_LINE(updateViaCache);
        entry->mRegistration.updateViaCache() =
            updateViaCache.ToInteger(&rv, 16);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        if (entry->mRegistration.updateViaCache() >
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE) {
          return NS_ERROR_INVALID_ARG;
        }

        nsAutoCString installedTimeStr;
        GET_LINE(installedTimeStr);
        int64_t installedTime = installedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerInstalledTime() = installedTime;

        nsAutoCString activatedTimeStr;
        GET_LINE(activatedTimeStr);
        int64_t activatedTime = activatedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerActivatedTime() = activatedTime;

        nsAutoCString lastUpdateTimeStr;
        GET_LINE(lastUpdateTimeStr);
        int64_t lastUpdateTime = lastUpdateTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.lastUpdateTime() = lastUpdateTime;

        nsAutoCString navigationPreloadEnabledStr;
        GET_LINE(navigationPreloadEnabledStr);
        bool navigationPreloadEnabled =
            navigationPreloadEnabledStr.ToInteger(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.navigationPreloadState().enabled() =
            navigationPreloadEnabled;

        GET_LINE(entry->mRegistration.navigationPreloadState().headerValue());

        if (version == SERVICEWORKERREGISTRAR_VERSION) {
          nsAutoCString expandoCountStr;
          GET_LINE(expandoCountStr);
          uint32_t expandoCount = expandoCountStr.ToInteger(&rv, 16);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          for (uint32_t expandoId = 0; expandoId < expandoCount; ++expandoId) {
            nsAutoCString key;
            GET_LINE(key);

            nsAutoCString value;
            GET_LINE(value);

            for (const ExpandoHandler& handler : mExpandoHandlers) {
              if (handler.mKey == key) {
                entry->mExpandos.AppendElement(
                    ExpandoData{key, value, &handler});
                break;
              }
            }
          }
        }

        break;
      }

      case 8: {
        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        nsAutoCString fetchFlag;
        GET_LINE(fetchFlag);
        if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
            !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
          return NS_ERROR_INVALID_ARG;
        }
        entry->mRegistration.currentWorkerHandlesFetch() =
            fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        nsAutoCString updateViaCache;
        GET_LINE(updateViaCache);
        entry->mRegistration.updateViaCache() =
            updateViaCache.ToInteger(&rv, 16);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        if (entry->mRegistration.updateViaCache() >
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE) {
          return NS_ERROR_INVALID_ARG;
        }

        nsAutoCString installedTimeStr;
        GET_LINE(installedTimeStr);
        int64_t installedTime = installedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerInstalledTime() = installedTime;

        nsAutoCString activatedTimeStr;
        GET_LINE(activatedTimeStr);
        int64_t activatedTime = activatedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerActivatedTime() = activatedTime;

        nsAutoCString lastUpdateTimeStr;
        GET_LINE(lastUpdateTimeStr);
        int64_t lastUpdateTime = lastUpdateTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.lastUpdateTime() = lastUpdateTime;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 7: {
        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        nsAutoCString fetchFlag;
        GET_LINE(fetchFlag);
        if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
            !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
          return NS_ERROR_INVALID_ARG;
        }
        entry->mRegistration.currentWorkerHandlesFetch() =
            fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        nsAutoCString loadFlags;
        GET_LINE(loadFlags);
        entry->mRegistration.updateViaCache() =
            loadFlags.ToInteger(&rv, 16) == nsIRequest::LOAD_NORMAL
                ? nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL
                : nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        nsAutoCString installedTimeStr;
        GET_LINE(installedTimeStr);
        int64_t installedTime = installedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerInstalledTime() = installedTime;

        nsAutoCString activatedTimeStr;
        GET_LINE(activatedTimeStr);
        int64_t activatedTime = activatedTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.currentWorkerActivatedTime() = activatedTime;

        nsAutoCString lastUpdateTimeStr;
        GET_LINE(lastUpdateTimeStr);
        int64_t lastUpdateTime = lastUpdateTimeStr.ToInteger64(&rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        entry->mRegistration.lastUpdateTime() = lastUpdateTime;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 6: {
        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        nsAutoCString fetchFlag;
        GET_LINE(fetchFlag);
        if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
            !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
          return NS_ERROR_INVALID_ARG;
        }
        entry->mRegistration.currentWorkerHandlesFetch() =
            fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        nsAutoCString loadFlags;
        GET_LINE(loadFlags);
        entry->mRegistration.updateViaCache() =
            loadFlags.ToInteger(&rv, 16) == nsIRequest::LOAD_NORMAL
                ? nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL
                : nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        entry->mRegistration.currentWorkerInstalledTime() = 0;
        entry->mRegistration.currentWorkerActivatedTime() = 0;
        entry->mRegistration.lastUpdateTime() = 0;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 5: {
        overwrite = true;
        dedupe = true;

        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        nsAutoCString fetchFlag;
        GET_LINE(fetchFlag);
        if (!fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE) &&
            !fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_FALSE)) {
          return NS_ERROR_INVALID_ARG;
        }
        entry->mRegistration.currentWorkerHandlesFetch() =
            fetchFlag.EqualsLiteral(SERVICEWORKERREGISTRAR_TRUE);

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        entry->mRegistration.updateViaCache() =
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        entry->mRegistration.currentWorkerInstalledTime() = 0;
        entry->mRegistration.currentWorkerActivatedTime() = 0;
        entry->mRegistration.lastUpdateTime() = 0;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 4: {
        overwrite = true;
        dedupe = true;

        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        // default handlesFetch flag to Enabled
        entry->mRegistration.currentWorkerHandlesFetch() = true;

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        entry->mRegistration.updateViaCache() =
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        entry->mRegistration.currentWorkerInstalledTime() = 0;
        entry->mRegistration.currentWorkerActivatedTime() = 0;
        entry->mRegistration.lastUpdateTime() = 0;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 3: {
        overwrite = true;
        dedupe = true;

        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration, true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        GET_LINE(entry->mRegistration.currentWorkerURL());

        // default handlesFetch flag to Enabled
        entry->mRegistration.currentWorkerHandlesFetch() = true;

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        entry->mRegistration.updateViaCache() =
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        entry->mRegistration.currentWorkerInstalledTime() = 0;
        entry->mRegistration.currentWorkerActivatedTime() = 0;
        entry->mRegistration.lastUpdateTime() = 0;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      case 2: {
        overwrite = true;
        dedupe = true;

        rv = CreatePrincipalInfo(lineInputStream, entry->mRegistration, true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        // scriptSpec is no more used in latest version.
        nsAutoCString unused;
        GET_LINE(unused);

        GET_LINE(entry->mRegistration.currentWorkerURL());

        // default handlesFetch flag to Enabled
        entry->mRegistration.currentWorkerHandlesFetch() = true;

        nsAutoCString cacheName;
        GET_LINE(cacheName);
        CopyUTF8toUTF16(cacheName, entry->mRegistration.cacheName());

        // waitingCacheName is no more used in latest version.
        GET_LINE(unused);

        entry->mRegistration.updateViaCache() =
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS;

        entry->mRegistration.currentWorkerInstalledTime() = 0;
        entry->mRegistration.currentWorkerActivatedTime() = 0;
        entry->mRegistration.lastUpdateTime() = 0;

        entry->mRegistration.navigationPreloadState() =
            gDefaultNavigationPreloadState;
        break;
      }

      default:
        MOZ_ASSERT_UNREACHABLE("Should never get here!");
    }

#undef GET_LINE

    rv = lineInputStream->ReadLine(line, &hasMoreLines);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!line.EqualsLiteral(SERVICEWORKERREGISTRAR_TERMINATOR)) {
      return NS_ERROR_FAILURE;
    }
  }

  stream->Close();

  // We currently only call this at startup where we block the main thread
  // preventing further operation until it completes, however take the lock
  // in case that changes

  nsTArray<ServiceWorkerData> registrationsWithExpandos;

  {
    MonitorAutoLock lock(mMonitor);
    // Copy data over to mData.
    for (uint32_t i = 0; i < tmpData.Length(); ++i) {
      // Older versions could sometimes write out empty, useless entries.
      // Prune those here.
      if (!ServiceWorkerRegistrationDataIsValid(tmpData[i].mRegistration)) {
        continue;
      }

      bool match = false;
      if (dedupe) {
        MOZ_ASSERT(overwrite);
        // If this is an old profile, then we might need to deduplicate.  In
        // theory this can be removed in the future (Bug 1248449)
        for (uint32_t j = 0; j < mData.Length(); ++j) {
          // Use same comparison as RegisterServiceWorker. Scope contains
          // basic origin information.  Combine with any principal attributes.
          if (Equivalent(tmpData[i].mRegistration, mData[j].mRegistration)) {
            // Last match wins, just like legacy loading used to do in
            // the ServiceWorkerManager.
            mData[j].mRegistration = tmpData[i].mRegistration;
            mData[j].mExpandos.Clear();
            // Dupe found, so overwrite file with reduced list.
            match = true;
            break;
          }
        }
      } else {
#ifdef DEBUG
        // Otherwise assert no duplications in debug builds.
        for (uint32_t j = 0; j < mData.Length(); ++j) {
          MOZ_ASSERT(
              !Equivalent(tmpData[i].mRegistration, mData[j].mRegistration));
        }
#endif
      }
      if (!match) {
        mData.AppendElement(tmpData[i]);

        if (!tmpData[i].mExpandos.IsEmpty()) {
          registrationsWithExpandos.AppendElement(tmpData[i]);
        }
      }
    }
  }

  if (!registrationsWithExpandos.IsEmpty()) {
    LoadExpandoCallbacks(registrationsWithExpandos);
  }

  // Overwrite previous version.
  // Cannot call SaveData directly because gtest uses main-thread.

  // XXX NOTE: if we could be accessed multi-threaded here, we would need to
  // find a way to lock around access to mData.  Since we can't, suppress the
  // thread-safety warnings.
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  if (overwrite && NS_FAILED(WriteData(mData))) {
    NS_WARNING("Failed to write data for the ServiceWorker Registations.");
    DeleteData();
  }
  MOZ_POP_THREAD_SAFETY

  return NS_OK;
}

void ServiceWorkerRegistrar::DeleteData() {
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);
    mData.Clear();

    if (!mProfileDir) {
      return;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsresult rv = file->Append(nsLiteralString(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = file->Remove(false);
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    return;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void ServiceWorkerRegistrar::RegisterServiceWorkerInternal(
    const ServiceWorkerRegistrationData& aData) {
  bool found = false;
  for (uint32_t i = 0, len = mData.Length(); i < len; ++i) {
    if (Equivalent(aData, mData[i].mRegistration)) {
      UpdateExpandoCallbacks(mData[i]);

      found = true;
      mData[i].mRegistration = aData;
      mData[i].mExpandos.Clear();
      break;
    }
  }

  if (!found) {
    MOZ_ASSERT(ServiceWorkerRegistrationDataIsValid(aData));
    mData.AppendElement(ServiceWorkerData{aData, nsTArray<ExpandoData>()});
  }

  mDataGeneration = GetNextGeneration();
}

class ServiceWorkerRegistrarSaveDataRunnable final : public Runnable {
  nsCOMPtr<nsIEventTarget> mEventTarget;
  const nsTArray<ServiceWorkerRegistrar::ServiceWorkerData> mData;
  const uint32_t mGeneration;

 public:
  ServiceWorkerRegistrarSaveDataRunnable(
      nsTArray<ServiceWorkerRegistrar::ServiceWorkerData>&& aData,
      uint32_t aGeneration)
      : Runnable("dom::ServiceWorkerRegistrarSaveDataRunnable"),
        mEventTarget(GetCurrentSerialEventTarget()),
        mData(std::move(aData)),
        mGeneration(aGeneration) {
    AssertIsOnBackgroundThread();
    MOZ_DIAGNOSTIC_ASSERT(mGeneration != kInvalidGeneration);
  }

  NS_IMETHOD
  Run() override {
    RefPtr<ServiceWorkerRegistrar> service = ServiceWorkerRegistrar::Get();
    MOZ_ASSERT(service);

    uint32_t fileGeneration = kInvalidGeneration;

    if (NS_SUCCEEDED(service->SaveData(mData))) {
      fileGeneration = mGeneration;
    }

    RefPtr<Runnable> runnable = NewRunnableMethod<uint32_t>(
        "ServiceWorkerRegistrar::DataSaved", service,
        &ServiceWorkerRegistrar::DataSaved, fileGeneration);
    MOZ_ALWAYS_SUCCEEDS(
        mEventTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));

    return NS_OK;
  }
};

void ServiceWorkerRegistrar::MaybeScheduleSaveData() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  if (mShuttingDown || mSaveDataRunnableDispatched ||
      mDataGeneration <= mFileGeneration) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  uint32_t generation = kInvalidGeneration;
  nsTArray<ServiceWorkerData> data;

  {
    MonitorAutoLock lock(mMonitor);
    generation = mDataGeneration;
    data.AppendElements(mData);
  }

  RefPtr<Runnable> runnable =
      new ServiceWorkerRegistrarSaveDataRunnable(std::move(data), generation);
  nsresult rv = target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);

  mSaveDataRunnableDispatched = true;
}

void ServiceWorkerRegistrar::ShutdownCompleted() {
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<nsresult> rv = GetShutdownPhase()->RemoveBlocker(this);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult ServiceWorkerRegistrar::SaveData(
    const nsTArray<ServiceWorkerData>& aData) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = WriteData(aData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to write data for the ServiceWorker Registations.");
    // Don't touch the file or in-memory state.  Writing files can
    // sometimes fail due to virus scanning, etc.  We should just leave
    // things as is so the next save operation can pick up any changes
    // without losing data.
  }
  return rv;
}

void ServiceWorkerRegistrar::DataSaved(uint32_t aFileGeneration) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mSaveDataRunnableDispatched);

  mSaveDataRunnableDispatched = false;

  // Check for shutdown before possibly triggering any more saves
  // runnables.
  MaybeScheduleShutdownCompleted();
  if (mShuttingDown) {
    return;
  }

  // If we got a valid generation, then the save was successful.
  if (aFileGeneration != kInvalidGeneration) {
    // Update the file generation.  We also check to see if we
    // can reset the generation back to zero if the file and data
    // are now in sync.  This allows us to avoid dealing with wrap
    // around of the generation count.
    mFileGeneration = aFileGeneration;
    MaybeResetGeneration();

    // Successful write resets the retry count.
    mRetryCount = 0;

    // Possibly schedule another save operation if more data
    // has come in while processing this one.
    MaybeScheduleSaveData();

    return;
  }

  // Otherwise, the save failed since the generation is invalid.  We
  // want to retry the save, but only a limited number of times.
  static const uint32_t kMaxRetryCount = 2;
  if (mRetryCount >= kMaxRetryCount) {
    return;
  }

  mRetryCount += 1;
  MaybeScheduleSaveData();
}

void ServiceWorkerRegistrar::MaybeScheduleShutdownCompleted() {
  AssertIsOnBackgroundThread();

  if (mSaveDataRunnableDispatched || !mShuttingDown) {
    return;
  }

  RefPtr<Runnable> runnable =
      NewRunnableMethod("dom::ServiceWorkerRegistrar::ShutdownCompleted", this,
                        &ServiceWorkerRegistrar::ShutdownCompleted);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
}

uint32_t ServiceWorkerRegistrar::GetNextGeneration() {
  uint32_t ret = mDataGeneration + 1;
  if (ret == kInvalidGeneration) {
    ret += 1;
  }
  return ret;
}

void ServiceWorkerRegistrar::MaybeResetGeneration() {
  if (mDataGeneration != mFileGeneration) {
    return;
  }
  mDataGeneration = mFileGeneration = 0;
}

bool ServiceWorkerRegistrar::IsSupportedVersion(uint32_t aVersion) const {
  uint32_t numVersions = std::size(gSupportedRegistrarVersions);
  for (uint32_t i = 0; i < numVersions; i++) {
    if (aVersion == gSupportedRegistrarVersions[i]) {
      return true;
    }
  }
  return false;
}

nsresult ServiceWorkerRegistrar::WriteData(
    const nsTArray<ServiceWorkerData>& aData) {
  // We cannot assert about the correct thread because normally this method
  // runs on a IO thread, but in gTests we call it from the main-thread.

  nsCOMPtr<nsIFile> file;

  {
    MonitorAutoLock lock(mMonitor);

    if (!mProfileDir) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = mProfileDir->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsresult rv = file->Append(nsLiteralString(SERVICEWORKERREGISTRAR_FILE));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewSafeLocalFileOutputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString buffer;
  buffer.AppendInt(static_cast<uint32_t>(SERVICEWORKERREGISTRAR_VERSION));
  buffer.Append('\n');

  uint32_t count;
  rv = stream->Write(buffer.Data(), buffer.Length(), &count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (count != buffer.Length()) {
    return NS_ERROR_UNEXPECTED;
  }

  for (const ServiceWorkerData& data : aData) {
    // We have an assertion further up the stack, but as a last
    // resort avoid writing out broken entries here.
    if (!ServiceWorkerRegistrationDataIsValid(data.mRegistration)) {
      continue;
    }

    const mozilla::ipc::PrincipalInfo& info = data.mRegistration.principal();

    MOZ_ASSERT(info.type() ==
               mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);

    const mozilla::ipc::ContentPrincipalInfo& cInfo =
        info.get_ContentPrincipalInfo();

    nsAutoCString suffix;
    cInfo.attrs().CreateSuffix(suffix);

    buffer.Truncate();
    buffer.Append(suffix.get());
    buffer.Append('\n');

    buffer.Append(data.mRegistration.scope());
    buffer.Append('\n');

    buffer.Append(data.mRegistration.currentWorkerURL());
    buffer.Append('\n');

    buffer.Append(data.mRegistration.currentWorkerHandlesFetch()
                      ? SERVICEWORKERREGISTRAR_TRUE
                      : SERVICEWORKERREGISTRAR_FALSE);
    buffer.Append('\n');

    buffer.Append(NS_ConvertUTF16toUTF8(data.mRegistration.cacheName()));
    buffer.Append('\n');

    buffer.AppendInt(data.mRegistration.updateViaCache(), 16);
    buffer.Append('\n');
    MOZ_DIAGNOSTIC_ASSERT(
        data.mRegistration.updateViaCache() ==
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_IMPORTS ||
        data.mRegistration.updateViaCache() ==
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_ALL ||
        data.mRegistration.updateViaCache() ==
            nsIServiceWorkerRegistrationInfo::UPDATE_VIA_CACHE_NONE);

    static_assert(nsIRequest::LOAD_NORMAL == 0,
                  "LOAD_NORMAL matches serialized value.");
    static_assert(nsIRequest::VALIDATE_ALWAYS == (1 << 11),
                  "VALIDATE_ALWAYS matches serialized value");

    buffer.AppendInt(data.mRegistration.currentWorkerInstalledTime());
    buffer.Append('\n');

    buffer.AppendInt(data.mRegistration.currentWorkerActivatedTime());
    buffer.Append('\n');

    buffer.AppendInt(data.mRegistration.lastUpdateTime());
    buffer.Append('\n');

    buffer.AppendInt(static_cast<int32_t>(
        data.mRegistration.navigationPreloadState().enabled()));
    buffer.Append('\n');

    buffer.Append(data.mRegistration.navigationPreloadState().headerValue());
    buffer.Append('\n');

    buffer.AppendInt(static_cast<uint32_t>(data.mExpandos.Length()), 16);
    buffer.Append('\n');

    for (const ExpandoData& expando : data.mExpandos) {
      buffer.Append(expando.mKey);
      buffer.Append('\n');
      buffer.Append(expando.mValue);
      buffer.Append('\n');
    }

    buffer.AppendLiteral(SERVICEWORKERREGISTRAR_TERMINATOR);
    buffer.Append('\n');

    rv = stream->Write(buffer.Data(), buffer.Length(), &count);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (count != buffer.Length()) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  nsCOMPtr<nsISafeOutputStream> safeStream = do_QueryInterface(stream);
  MOZ_ASSERT(safeStream);

  rv = safeStream->Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void ServiceWorkerRegistrar::ProfileStarted() {
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);
  MOZ_DIAGNOSTIC_ASSERT(!mProfileDir);

  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(mProfileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoString blockerName;
  MOZ_ALWAYS_SUCCEEDS(GetName(blockerName));

  rv = GetShutdownPhase()->AddBlocker(
      this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, blockerName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target, "Must have stream transport service");

  nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("dom::ServiceWorkerRegistrar::LoadData", this,
                        &ServiceWorkerRegistrar::LoadData);
  rv = target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the LoadDataRunnable.");
  }
}

void ServiceWorkerRegistrar::ProfileStopped() {
  MOZ_ASSERT(NS_IsMainThread());

  MonitorAutoLock lock(mMonitor);

  if (!mProfileDir) {
    nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                         getter_AddRefs(mProfileDir));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // If we do not have a profile directory, we are somehow screwed.
      MOZ_DIAGNOSTIC_ASSERT(
          false,
          "NS_GetSpecialDirectory for NS_APP_USER_PROFILE_50_DIR failed!");
    }
  }

  // Mutations to the ServiceWorkerRegistrar happen on the PBackground thread,
  // issued by the ServiceWorkerManagerService, so the appropriate place to
  // trigger shutdown is on that thread.
  //
  // However, it's quite possible that the PBackground thread was not brought
  // into existence for xpcshell tests.  We don't cause it to be created
  // ourselves for any reason, for example.
  //
  // In this scenario, we know that:
  // - We will receive exactly one call to ourself from BlockShutdown() and
  //   BlockShutdown() will be called (at most) once.
  // - The only way our Shutdown() method gets called is via
  //   BackgroundParentImpl::RecvShutdownServiceWorkerRegistrar() being
  //   invoked, which only happens if we get to that send below here that we
  //   can't get to.
  // - All Shutdown() does is set mShuttingDown=true (essential for
  //   invariants) and invoke MaybeScheduleShutdownCompleted().
  // - Since there is no PBackground thread, mSaveDataRunnableDispatched must
  //   be false because only MaybeScheduleSaveData() set it and it only runs
  //   on the background thread, so it cannot have run.  And so we would
  //   expect MaybeScheduleShutdownCompleted() to schedule an invocation of
  //   ShutdownCompleted on the main thread.
  PBackgroundChild* child = BackgroundChild::GetForCurrentThread();
  if (mProfileDir && child) {
    if (child->SendShutdownServiceWorkerRegistrar()) {
      // Normal shutdown sequence has been initiated, go home.
      return;
    }
    // If we get here, the PBackground thread has probably gone nuts and we
    // want to know it.
    MOZ_DIAGNOSTIC_ASSERT(
        false, "Unable to send the ShutdownServiceWorkerRegistrar message.");
  }

  // On any error it's appropriate to set mShuttingDown=true (as Shutdown
  // would do) and directly invoke ShutdownCompleted() (as Shutdown would
  // indirectly do via MaybeScheduleShutdownCompleted) in order to unblock
  // shutdown.
  mShuttingDown = true;
  ShutdownCompleted();
}

// Async shutdown blocker methods

NS_IMETHODIMP
ServiceWorkerRegistrar::BlockShutdown(nsIAsyncShutdownClient* aClient) {
  ProfileStopped();
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrar::GetName(nsAString& aName) {
  aName = u"ServiceWorkerRegistrar: Flushing data"_ns;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrar::GetState(nsIPropertyBag** aBagOut) {
  nsCOMPtr<nsIWritablePropertyBag2> propertyBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");

  MOZ_TRY(propertyBag->SetPropertyAsBool(u"shuttingDown"_ns, mShuttingDown));

  MOZ_TRY(propertyBag->SetPropertyAsBool(u"saveDataRunnableDispatched"_ns,
                                         mSaveDataRunnableDispatched));

  propertyBag.forget(aBagOut);

  return NS_OK;
}

#define RELEASE_ASSERT_SUCCEEDED(rv, name)                                    \
  do {                                                                        \
    if (NS_FAILED(rv)) {                                                      \
      if ((rv) == NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS) {               \
        if (auto* context = CycleCollectedJSContext::Get()) {                 \
          if (RefPtr<Exception> exn = context->GetPendingException()) {       \
            MOZ_CRASH_UNSAFE_PRINTF("Failed to get " name ": %s",             \
                                    exn->GetMessageMoz().get());              \
          }                                                                   \
        }                                                                     \
      }                                                                       \
                                                                              \
      nsAutoCString errorName;                                                \
      GetErrorName(rv, errorName);                                            \
      MOZ_CRASH_UNSAFE_PRINTF("Failed to get " name ": %s", errorName.get()); \
    }                                                                         \
  } while (0)

nsCOMPtr<nsIAsyncShutdownClient> ServiceWorkerRegistrar::GetShutdownPhase()
    const {
  nsresult rv;
  nsCOMPtr<nsIAsyncShutdownService> svc =
      do_GetService("@mozilla.org/async-shutdown-service;1", &rv);
  // If this fails, something is very wrong on the JS side (or we're out of
  // memory), and there's no point in continuing startup. Include as much
  // information as possible in the crash report.
  RELEASE_ASSERT_SUCCEEDED(rv, "async shutdown service");

  nsCOMPtr<nsIAsyncShutdownClient> client;
  rv = svc->GetProfileBeforeChange(getter_AddRefs(client));
  RELEASE_ASSERT_SUCCEEDED(rv, "profileBeforeChange shutdown blocker");
  return client;
}

#undef RELEASE_ASSERT_SUCCEEDED

void ServiceWorkerRegistrar::Shutdown() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShuttingDown);

  mShuttingDown = true;
  MaybeScheduleShutdownCompleted();
}

NS_IMETHODIMP
ServiceWorkerRegistrar::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "profile-after-change")) {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    observerService->RemoveObserver(this, "profile-after-change");

    // The profile is fully loaded, now we can proceed with the loading of data
    // from disk.
    ProfileStarted();

    return NS_OK;
  }

  MOZ_ASSERT(false, "ServiceWorkerRegistrar got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void ServiceWorkerRegistrar::LoadExpandoCallbacks(
    const CopyableTArray<ServiceWorkerData>& aData) {
  if (NS_IsMainThread()) {
    for (const ServiceWorkerData& data : aData) {
      for (const ExpandoData& expando : data.mExpandos) {
        MOZ_ASSERT(expando.mHandler);
        expando.mHandler->mServiceWorkerLoaded(data.mRegistration,
                                               expando.mValue);
      }
    }

    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [self = RefPtr{this}, aData] { self->LoadExpandoCallbacks(aData); }));
}

void ServiceWorkerRegistrar::UpdateExpandoCallbacks(
    const ServiceWorkerData& aData) {
  if (NS_IsMainThread()) {
    for (const ExpandoData& expando : aData.mExpandos) {
      MOZ_ASSERT(expando.mHandler);
      expando.mHandler->mServiceWorkerUpdated(aData.mRegistration);
    }

    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      __func__,
      [self = RefPtr{this}, aData] { self->UpdateExpandoCallbacks(aData); }));
}

void ServiceWorkerRegistrar::UnregisterExpandoCallbacks(
    const CopyableTArray<ServiceWorkerData>& aData) {
  if (NS_IsMainThread()) {
    for (const ServiceWorkerData& data : aData) {
      for (const ExpandoData& expando : data.mExpandos) {
        MOZ_ASSERT(expando.mHandler);
        expando.mHandler->mServiceWorkerUnregistered(data.mRegistration);
      }
    }

    return;
  }

  NS_DispatchToMainThread(
      NS_NewRunnableFunction(__func__, [self = RefPtr{this}, aData] {
        self->UnregisterExpandoCallbacks(aData);
      }));
}

}  // namespace mozilla::dom
