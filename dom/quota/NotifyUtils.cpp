/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/NotifyUtils.h"

#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/quota/NotifyUtilsCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsError.h"
#include "nsSupportsPrimitives.h"

namespace mozilla::dom::quota {

void NotifyStoragePressure(QuotaManager& aQuotaManager, uint64_t aUsage) {
  aQuotaManager.AssertNotCurrentThreadOwnsQuotaMutex();

  auto subjectGetter = [usage = aUsage]() {
    auto wrapper = MakeRefPtr<nsSupportsPRUint64>();

    MOZ_ALWAYS_SUCCEEDS(wrapper->SetData(usage));

    return wrapper.forget();
  };

  NotifyObserversOnMainThread("QuotaManager::StoragePressure",
                              std::move(subjectGetter));
}

void NotifyMaintenanceStarted(QuotaManager& aQuotaManager) {
  aQuotaManager.AssertIsOnOwningThread();

  if (!StaticPrefs::dom_quotaManager_testing()) {
    return;
  }

  NotifyObserversOnMainThread("QuotaManager::MaintenanceStarted");
}

void NotifyClientDirectoryOpeningStarted(QuotaManager& aQuotaManager) {
  aQuotaManager.AssertIsOnOwningThread();

  if (!StaticPrefs::dom_quotaManager_testing()) {
    return;
  }

  NotifyObserversOnMainThread("QuotaManager::ClientDirectoryOpeningStarted");
}

}  // namespace mozilla::dom::quota
