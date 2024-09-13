/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/NotifyUtils.h"

#include "mozilla/Services.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::quota {

namespace {

class StoragePressureRunnable final : public Runnable {
  const uint64_t mUsage;

 public:
  explicit StoragePressureRunnable(uint64_t aUsage)
      : Runnable("dom::quota::QuotaObject::StoragePressureRunnable"),
        mUsage(aUsage) {}

 private:
  ~StoragePressureRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

}  // namespace

void NotifyStoragePressure(QuotaManager& aQuotaManager, uint64_t aUsage) {
  aQuotaManager.AssertNotCurrentThreadOwnsQuotaMutex();

  RefPtr<StoragePressureRunnable> storagePressureRunnable =
      new StoragePressureRunnable(aUsage);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(storagePressureRunnable));
}

void NotifyMaintenanceStarted(QuotaManager& aQuotaManager) {
  aQuotaManager.AssertIsOnOwningThread();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(NS_NewRunnableFunction(
      "dom::quota::QuotaManager::NotifyMaintenanceStarted", []() {
        nsCOMPtr<nsIObserverService> observerService =
            mozilla::services::GetObserverService();
        QM_TRY(MOZ_TO_RESULT(observerService), QM_VOID);

        observerService->NotifyObservers(
            nullptr, "QuotaManager::MaintenanceStarted", u"");
      })));
}

NS_IMETHODIMP
StoragePressureRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (NS_WARN_IF(!wrapper)) {
    return NS_ERROR_FAILURE;
  }

  wrapper->SetData(mUsage);

  obsSvc->NotifyObservers(wrapper, "QuotaManager::StoragePressure", u"");

  return NS_OK;
}

}  // namespace mozilla::dom::quota
