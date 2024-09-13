/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_NOTIFYUTILS_H_
#define DOM_QUOTA_NOTIFYUTILS_H_

#include <cstdint>

namespace mozilla::dom::quota {

class QuotaManager;

void NotifyStoragePressure(QuotaManager& aQuotaManager, uint64_t aUsage);

void NotifyMaintenanceStarted(QuotaManager& aQuotaManager);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_NOTIFYUTILS_H_
