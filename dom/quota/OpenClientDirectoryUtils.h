/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_OPENCLIENTDIRECTORYUTILS_H_
#define DOM_QUOTA_OPENCLIENTDIRECTORYUTILS_H_

#include <functional>

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/ArtificialFailure.h"
#include "mozilla/dom/quota/ClientDirectoryLock.h"
#include "mozilla/dom/quota/ClientDirectoryLockHandle.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLockInlines.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UniversalDirectoryLock.h"

namespace mozilla::dom::quota {

template <typename UninitChecker, typename PromiseArrayIter>
RefPtr<UniversalDirectoryLock> CreateDirectoryLockForInitialization(
    QuotaManager& aQuotaManager, const PersistenceScope& aPersistenceScope,
    const OriginScope& aOriginScope, const bool aAlreadyInitialized,
    UninitChecker&& aUninitChecker, PromiseArrayIter&& aPromiseArrayIter) {
  RefPtr<UniversalDirectoryLock> directoryLock =
      aQuotaManager.CreateDirectoryLockInternal(
          aPersistenceScope, aOriginScope, ClientStorageScope::CreateFromNull(),
          /* aExclusive */ false);

  auto prepareInfo = directoryLock->Prepare();

  if (aAlreadyInitialized &&
      !std::forward<UninitChecker>(aUninitChecker)(prepareInfo)) {
    return nullptr;
  }

  auto iter = std::forward<PromiseArrayIter>(aPromiseArrayIter);
  *iter = directoryLock->Acquire(std::move(prepareInfo));
  ++iter;

  return directoryLock;
}

template <typename Callable>
class MaybeInitializeHelper final {
 public:
  MaybeInitializeHelper(RefPtr<UniversalDirectoryLock> aDirectoryLock,
                        Callable&& aCallable)
      : mDirectoryLock(std::move(aDirectoryLock)),
        mCallable(std::move(aCallable)) {}

  RefPtr<BoolPromise> operator()(
      const BoolPromise::ResolveOrRejectValue& aValue) {
    if (aValue.IsReject()) {
      SafeDropDirectoryLockIfNotDropped(mDirectoryLock);

      return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
    }

    if (!mDirectoryLock) {
      return BoolPromise::CreateAndResolve(true, __func__);
    }

    return mCallable(std::move(mDirectoryLock));
  }

 private:
  RefPtr<UniversalDirectoryLock> mDirectoryLock;
  Callable mCallable;
};

template <typename Callable>
auto MaybeInitialize(RefPtr<UniversalDirectoryLock> aDirectoryLock,
                     Callable&& aCallable) {
  return MaybeInitializeHelper(std::move(aDirectoryLock),
                               std::forward<Callable>(aCallable));
}

auto MaybeInitialize(RefPtr<UniversalDirectoryLock> aDirectoryLock,
                     RefPtr<QuotaManager> aQuotaManager,
                     RefPtr<BoolPromise> (QuotaManager::*aMethod)(
                         RefPtr<UniversalDirectoryLock>)) {
  return MaybeInitializeHelper(
      std::move(aDirectoryLock),
      [quotaManager = std::move(aQuotaManager),
       method = aMethod](RefPtr<UniversalDirectoryLock> aDirectoryLock) {
        return (quotaManager->*method)(std::move(aDirectoryLock));
      });
}

// XXX: Consider renaming the finalization class and function below to make
// their purpose more specific
template <typename Callable>
class MaybeFinalizeHelper final {
 public:
  MaybeFinalizeHelper(RefPtr<ClientDirectoryLock> aClientDirectoryLock,
                      Callable&& aCallable)
      : mClientDirectoryLock(std::move(aClientDirectoryLock)),
        mCallable(std::move(aCallable)) {}

  RefPtr<QuotaManager::ClientDirectoryLockHandlePromise> operator()(
      const BoolPromise::ResolveOrRejectValue& aValue) {
    if (aValue.IsReject()) {
      DropDirectoryLockIfNotDropped(mClientDirectoryLock);

      return QuotaManager::ClientDirectoryLockHandlePromise::CreateAndReject(
          aValue.RejectValue(), __func__);
    }

    QM_TRY(ArtificialFailure(
               nsIQuotaArtificialFailure::CATEGORY_OPEN_CLIENT_DIRECTORY),
           [this](nsresult rv) {
             DropDirectoryLock(mClientDirectoryLock);

             return QuotaManager::ClientDirectoryLockHandlePromise::
                 CreateAndReject(rv, __func__);
           });

    return mCallable(std::move(mClientDirectoryLock));
  }

 private:
  RefPtr<ClientDirectoryLock> mClientDirectoryLock;
  Callable mCallable;
};

template <typename Callable>
auto MaybeFinalize(RefPtr<ClientDirectoryLock> aClientDirectoryLock,
                   Callable&& aCallable) {
  return MaybeFinalizeHelper(std::move(aClientDirectoryLock),
                             std::forward<Callable>(aCallable));
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_OPENCLIENTDIRECTORYUTILS_H_
