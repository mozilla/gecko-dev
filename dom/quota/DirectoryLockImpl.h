/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCKIMPL_H_
#define DOM_QUOTA_DIRECTORYLOCKIMPL_H_

#include <cstdint>
#include <functional>
#include <utility>

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "mozilla/Assertions.h"
#include "mozilla/MozPromise.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLockCategory.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsCOMPtr.h"

class nsITimer;

namespace mozilla::dom::quota {

struct OriginMetadata;
class QuotaManager;

enum class ShouldUpdateLockIdTableFlag { No, Yes };

class DirectoryLockImpl : public ClientDirectoryLock {
  friend class QuotaManager;
  friend class UniversalDirectoryLock;

  const NotNull<RefPtr<QuotaManager>> mQuotaManager;

  const PersistenceScope mPersistenceScope;
  const OriginScope mOriginScope;
  const Nullable<Client::Type> mClientType;

  MozPromiseHolder<BoolPromise> mAcquirePromiseHolder;
  nsCOMPtr<nsITimer> mAcquireTimer;

  nsTArray<NotNull<DirectoryLockImpl*>> mBlocking;
  nsTArray<NotNull<DirectoryLockImpl*>> mBlockedOn;

  std::function<void()> mInvalidateCallback;

  const int64_t mId;

  const bool mExclusive;

  // Internal quota manager operations use this flag to prevent directory lock
  // registraction/unregistration from updating origin access time, etc.
  const bool mInternal;

  const bool mShouldUpdateLockIdTable;

  const DirectoryLockCategory mCategory;

  bool mRegistered;
  FlippedOnce<true> mPending;
  FlippedOnce<false> mAcquired;
  FlippedOnce<false> mInvalidated;
  FlippedOnce<false> mDropped;

 public:
  DirectoryLockImpl(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                    const PersistenceScope& aPersistenceScope,
                    const OriginScope& aOriginScope,
                    const Nullable<Client::Type>& aClientType, bool aExclusive,
                    bool aInternal,
                    ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag,
                    DirectoryLockCategory aCategory);

  // DirectoryLock interface

  NS_INLINE_DECL_REFCOUNTING(DirectoryLockImpl, override)

  int64_t Id() const override { return mId; }

  const PersistenceScope& PersistenceScopeRef() const override {
    return mPersistenceScope;
  }

  const OriginScope& GetOriginScope() const override { return mOriginScope; }

  const Nullable<Client::Type>& NullableClientType() const override {
    return mClientType;
  }

  DirectoryLockCategory Category() const override { return mCategory; }

  bool Acquired() const override { return mAcquired; }

  bool MustWait() const override;

  nsTArray<RefPtr<DirectoryLock>> LocksMustWaitFor() const override;

  bool Invalidated() const override { return mInvalidated; }

  bool Dropped() const override { return mDropped; }

  RefPtr<BoolPromise> Acquire() override;

  void AcquireImmediately() override;

  void AssertIsAcquiredExclusively() override
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  RefPtr<BoolPromise> Drop() override;

  void OnInvalidate(std::function<void()>&& aCallback) override;

  void Log() const override;

  // OriginDirectoryLock interface

  PersistenceType GetPersistenceType() const override {
    MOZ_DIAGNOSTIC_ASSERT(mPersistenceScope.IsValue());

    return mPersistenceScope.GetValue();
  }

  quota::OriginMetadata OriginMetadata() const override {
    MOZ_DIAGNOSTIC_ASSERT(mOriginScope.IsOrigin());

    return quota::OriginMetadata{mOriginScope.GetPrincipalMetadata(),
                                 GetPersistenceType()};
  }

  const nsACString& Origin() const override {
    MOZ_DIAGNOSTIC_ASSERT(mOriginScope.IsOrigin());
    MOZ_DIAGNOSTIC_ASSERT(!mOriginScope.GetOrigin().IsEmpty());

    return mOriginScope.GetOrigin();
  }

  // ClientDirectoryLock interface

  Client::Type ClientType() const override {
    MOZ_DIAGNOSTIC_ASSERT(!mClientType.IsNull());
    MOZ_DIAGNOSTIC_ASSERT(mClientType.Value() < Client::TypeMax());

    return mClientType.Value();
  }

 private:
  virtual ~DirectoryLockImpl();

  static RefPtr<ClientDirectoryLock> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata, Client::Type aClientType,
      bool aExclusive) {
    return Create(std::move(aQuotaManager),
                  PersistenceScope::CreateFromValue(aPersistenceType),
                  OriginScope::FromOrigin(aOriginMetadata),
                  Nullable<Client::Type>(aClientType), aExclusive, false,
                  ShouldUpdateLockIdTableFlag::Yes,
                  DirectoryLockCategory::None);
  }

  static RefPtr<OriginDirectoryLock> CreateForEviction(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata) {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
    MOZ_ASSERT(!aOriginMetadata.mOrigin.IsEmpty());
    MOZ_ASSERT(!aOriginMetadata.mStorageOrigin.IsEmpty());

    return Create(
        std::move(aQuotaManager),
        PersistenceScope::CreateFromValue(aPersistenceType),
        OriginScope::FromOrigin(aOriginMetadata), Nullable<Client::Type>(),
        /* aExclusive */ true, /* aInternal */ true,
        ShouldUpdateLockIdTableFlag::No, DirectoryLockCategory::UninitOrigins);
  }

  static RefPtr<DirectoryLockImpl> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PersistenceScope& aPersistenceScope,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      bool aInternal, ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag,
      DirectoryLockCategory aCategory) {
    MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
    MOZ_ASSERT_IF(!aInternal, aPersistenceScope.IsValue());
    MOZ_ASSERT_IF(!aInternal,
                  aPersistenceScope.GetValue() != PERSISTENCE_TYPE_INVALID);
    MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
    MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
    MOZ_ASSERT_IF(!aInternal, aClientType.Value() < Client::TypeMax());

    return MakeRefPtr<DirectoryLockImpl>(
        std::move(aQuotaManager), aPersistenceScope, aOriginScope, aClientType,
        aExclusive, aInternal, aShouldUpdateLockIdTableFlag, aCategory);
  }

  void AssertIsOnOwningThread() const
#ifdef DEBUG
      ;
#else
  {
  }
#endif

  bool IsInternal() const { return mInternal; }

  void SetRegistered(bool aRegistered) { mRegistered = aRegistered; }

  bool IsPending() const { return mPending; }

  // Ideally, we would have just one table (instead of these two:
  // QuotaManager::mDirectoryLocks and QuotaManager::mDirectoryLockIdTable) for
  // all registered locks. However, some directory locks need to be accessed off
  // the PBackground thread, so the access must be protected by the quota mutex.
  // The problem is that directory locks for eviction must be currently created
  // while the mutex lock is already acquired. So we decided to have two tables
  // for now and to not register directory locks for eviction in
  // QuotaManager::mDirectoryLockIdTable. This can be improved in future after
  // some refactoring of the mutex locking.
  bool ShouldUpdateLockIdTable() const { return mShouldUpdateLockIdTable; }

  bool ShouldUpdateLockTable() {
    return !mInternal &&
           mPersistenceScope.GetValue() != PERSISTENCE_TYPE_PERSISTENT;
  }

  bool Overlaps(const DirectoryLockImpl& aLock) const;

  // Test whether this DirectoryLock needs to wait for the given lock.
  bool MustWaitFor(const DirectoryLockImpl& aLock) const;

  void AddBlockingLock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlocking.AppendElement(WrapNotNull(&aLock));
  }

  const nsTArray<NotNull<DirectoryLockImpl*>>& GetBlockedOnLocks() {
    return mBlockedOn;
  }

  void AddBlockedOnLock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.AppendElement(WrapNotNull(&aLock));
  }

  void MaybeUnblock(DirectoryLockImpl& aLock) {
    AssertIsOnOwningThread();

    mBlockedOn.RemoveElement(&aLock);
    if (mBlockedOn.IsEmpty()) {
      NotifyOpenListener();
    }
  }

  void NotifyOpenListener();

  void AcquireInternal();

  void Invalidate();

  void Unregister();
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCKIMPL_H_
