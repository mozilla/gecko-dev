/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UniversalDirectoryLock.h"

#include <utility>

#include "nsDebug.h"
#include "nsString.h"
#include "mozilla/Assertions.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/ClientDirectoryLock.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

RefPtr<ClientDirectoryLock> UniversalDirectoryLock::SpecializeForClient(
    PersistenceType aPersistenceType,
    const quota::OriginMetadata& aOriginMetadata,
    Client::Type aClientType) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT(!aOriginMetadata.mGroup.IsEmpty());
  MOZ_ASSERT(!aOriginMetadata.mOrigin.IsEmpty());
  MOZ_ASSERT(aClientType < Client::TypeMax());
  MOZ_ASSERT(mAcquirePromiseHolder.IsEmpty());
  MOZ_ASSERT(mBlockedOn.IsEmpty());

  if (NS_WARN_IF(mExclusive)) {
    return nullptr;
  }

  RefPtr<ClientDirectoryLock> lock = ClientDirectoryLock::Create(
      mQuotaManager, PersistenceScope::CreateFromValue(aPersistenceType),
      OriginScope::FromOrigin(aOriginMetadata),
      Nullable<Client::Type>(aClientType),
      /* aExclusive */ false, mInternal, ShouldUpdateLockIdTableFlag::Yes,
      mCategory);
  if (NS_WARN_IF(!Overlaps(*lock))) {
    return nullptr;
  }

#ifdef DEBUG
  for (DirectoryLockImpl* const existingLock :
       Reversed(mQuotaManager->mDirectoryLocks)) {
    if (existingLock != this && !existingLock->MustWaitFor(*this)) {
      MOZ_ASSERT(!existingLock->MustWaitFor(*lock));
    }
  }
#endif

  for (const auto& blockedLock : mBlocking) {
    if (blockedLock->MustWaitFor(*lock)) {
      lock->AddBlockingLock(*blockedLock);
      blockedLock->AddBlockedOnLock(*lock);
    }
  }

  mQuotaManager->RegisterDirectoryLock(*lock);

  if (mInvalidated) {
    lock->Invalidate();
  }

  return lock;
}

// static
RefPtr<UniversalDirectoryLock> UniversalDirectoryLock::CreateInternal(
    MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
    const PersistenceScope& aPersistenceScope, const OriginScope& aOriginScope,
    const Nullable<Client::Type>& aClientType, bool aExclusive,
    DirectoryLockCategory aCategory) {
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());

  return MakeRefPtr<UniversalDirectoryLock>(
      std::move(aQuotaManager), aPersistenceScope, aOriginScope, aClientType,
      aExclusive, true, ShouldUpdateLockIdTableFlag::Yes, aCategory);
}

}  // namespace mozilla::dom::quota
