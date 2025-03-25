/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerRegistrar_h
#define mozilla_dom_ServiceWorkerRegistrar_h

#include "mozilla/Monitor.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "nsClassHashtable.h"
#include "nsIAsyncShutdown.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

#define SERVICEWORKERREGISTRAR_FILE u"serviceworker.txt"
#define SERVICEWORKERREGISTRAR_VERSION 10
#define SERVICEWORKERREGISTRAR_TERMINATOR "#"
#define SERVICEWORKERREGISTRAR_TRUE "true"
#define SERVICEWORKERREGISTRAR_FALSE "false"

class nsIFile;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

}  // namespace mozilla

namespace mozilla::dom {

class ServiceWorkerRegistrar : public nsIObserver,
                               public nsIAsyncShutdownBlocker {
  friend class ServiceWorkerRegistrarSaveDataRunnable;

  // The internal data struct is public to make gtests happy.
 public:
  // An expando handler consists of a set of callbacks and a key. During
  // serialization/deserialization, ServiceWorkerRegistrar triggers these
  // callbacks based on the key name found on disk.
  struct ExpandoHandler {
    nsCString mKey;
    // The deserialization of the value is up to this callback.
    void (*mServiceWorkerLoaded)(const ServiceWorkerRegistrationData& aData,
                                 const nsACString& aValue);
    void (*mServiceWorkerUpdated)(const ServiceWorkerRegistrationData& aData);
    void (*mServiceWorkerUnregistered)(
        const ServiceWorkerRegistrationData& aData);
  };

  struct ExpandoData {
    nsCString mKey;
    nsCString mValue;
    const ExpandoHandler* mHandler;
  };

  struct ServiceWorkerData {
    ServiceWorkerRegistrationData mRegistration;
    CopyableTArray<ExpandoData> mExpandos;
  };

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER

  static void Initialize();

  void Shutdown();

  void DataSaved(uint32_t aFileGeneration);

  static already_AddRefed<ServiceWorkerRegistrar> Get();

  void GetRegistrations(nsTArray<ServiceWorkerRegistrationData>& aValues);

  void RegisterServiceWorker(const ServiceWorkerRegistrationData& aData);
  void UnregisterServiceWorker(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
      const nsACString& aScope);

  // Add or overwrite an expando key/value to a SW registration.
  void StoreServiceWorkerExpandoOnMainThread(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
      const nsACString& aScope, const nsACString& aKey,
      const nsACString& aValue);

  // Remove an existing expando key from a SW registration.
  // This method is main-thread only.
  void UnstoreServiceWorkerExpandoOnMainThread(
      const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
      const nsACString& aScope, const nsACString& aKey);

  void RemoveAll();

  bool ReloadDataForTest();

 protected:
  // These methods are protected because we test this class using gTest
  // subclassing it.
  void LoadData();
  nsresult SaveData(const nsTArray<ServiceWorkerData>& aData);

  nsresult ReadData();
  nsresult WriteData(const nsTArray<ServiceWorkerData>& aData);
  void DeleteData();

  void RegisterServiceWorkerInternal(const ServiceWorkerRegistrationData& aData)
      MOZ_REQUIRES(mMonitor);

  ServiceWorkerRegistrar();
  virtual ~ServiceWorkerRegistrar();

 private:
  void ProfileStarted();
  void ProfileStopped();

  void MaybeScheduleSaveData();
  void ShutdownCompleted();
  void MaybeScheduleShutdownCompleted();

  uint32_t GetNextGeneration();
  void MaybeResetGeneration();

  nsCOMPtr<nsIAsyncShutdownClient> GetShutdownPhase() const;

  bool IsSupportedVersion(uint32_t aVersion) const;

  void LoadExpandoCallbacks(const CopyableTArray<ServiceWorkerData>& aData);
  void UpdateExpandoCallbacks(const ServiceWorkerData& aData);
  void UnregisterExpandoCallbacks(
      const CopyableTArray<ServiceWorkerData>& aData);

 protected:
  mozilla::Monitor mMonitor;

  // protected by mMonitor.
  nsCOMPtr<nsIFile> mProfileDir MOZ_GUARDED_BY(mMonitor);
  // Read on mainthread, modified on background thread EXCEPT for
  // ReloadDataForTest() AND for gtest, which modifies this on MainThread.
  nsTArray<ServiceWorkerData> mData MOZ_GUARDED_BY(mMonitor);
  bool mDataLoaded MOZ_GUARDED_BY(mMonitor);

  // PBackground thread only
  uint32_t mDataGeneration;
  uint32_t mFileGeneration;
  uint32_t mRetryCount;
  bool mShuttingDown;
  bool mSaveDataRunnableDispatched;

  nsTArray<ExpandoHandler> mExpandoHandlers;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_ServiceWorkerRegistrar_h
