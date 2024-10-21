/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginDirectoryLock.h"

#include <utility>

#include "nsString.h"
#include "mozilla/Assertions.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/DirectoryLockCategory.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

// static
RefPtr<OriginDirectoryLock> OriginDirectoryLock::CreateForEviction(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata) {
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT(!aOriginMetadata.mOrigin.IsEmpty());
  MOZ_ASSERT(!aOriginMetadata.mStorageOrigin.IsEmpty());

  return MakeRefPtr<OriginDirectoryLock>(
      std::move(aQuotaManager),
      PersistenceScope::CreateFromValue(aPersistenceType),
      OriginScope::FromOrigin(aOriginMetadata), Nullable<Client::Type>(),
      /* aExclusive */ true, /* aInternal */ true,
      ShouldUpdateLockIdTableFlag::No, DirectoryLockCategory::UninitOrigins);
}

}  // namespace mozilla::dom::quota
