/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientDirectoryLock.h"

#include <utility>

#include "nsString.h"
#include "mozilla/Assertions.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/DirectoryLockCategory.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

// static
RefPtr<ClientDirectoryLock> ClientDirectoryLock::Create(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata, Client::Type aClientType,
    bool aExclusive) {
  return MakeRefPtr<ClientDirectoryLock>(
      std::move(aQuotaManager),
      PersistenceScope::CreateFromValue(aPersistenceType),
      OriginScope::FromOrigin(aOriginMetadata),
      Nullable<Client::Type>(aClientType), aExclusive, false,
      ShouldUpdateLockIdTableFlag::Yes, DirectoryLockCategory::None);
}

// static
RefPtr<ClientDirectoryLock> ClientDirectoryLock::Create(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PersistenceScope& aPersistenceScope, const OriginScope& aOriginScope,
    const Nullable<Client::Type>& aClientType, bool aExclusive, bool aInternal,
    ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag,
    DirectoryLockCategory aCategory) {
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
  MOZ_ASSERT_IF(!aInternal, aPersistenceScope.IsValue());
  MOZ_ASSERT_IF(!aInternal,
                aPersistenceScope.GetValue() != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() < Client::TypeMax());

  return MakeRefPtr<ClientDirectoryLock>(
      std::move(aQuotaManager), aPersistenceScope, aOriginScope, aClientType,
      aExclusive, aInternal, aShouldUpdateLockIdTableFlag, aCategory);
}

}  // namespace mozilla::dom::quota
