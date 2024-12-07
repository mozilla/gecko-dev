/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaParent.h"

#include <mozilla/Assertions.h>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/ErrorHandling.h"
#include "mozilla/dom/quota/PrincipalUtils.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"
#include "mozilla/dom/quota/QuotaUsageRequestParent.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsDebug.h"
#include "nsError.h"
#include "OriginOperations.h"
#include "QuotaRequestBase.h"

// CUF == CRASH_UNLESS_FUZZING
#define QM_CUF_AND_IPC_FAIL(actor)                           \
  [&_actor = *actor](const auto& aFunc, const auto& aExpr) { \
    MOZ_CRASH_UNLESS_FUZZING();                              \
    return QM_IPC_FAIL(&_actor)(aFunc, aExpr);               \
  }

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

namespace {

template <typename PromiseType, typename ResolverType, bool MoveOnly>
class PromiseResolveOrRejectCallbackBase {
 public:
  PromiseResolveOrRejectCallbackBase(RefPtr<Quota> aQuota,
                                     ResolverType&& aResolver)
      : mResolver(std::move(aResolver)), mQuota(std::move(aQuota)) {}

 protected:
  bool CanSend() const { return mQuota->CanSend(); }

  ResolverType mResolver;

 private:
  RefPtr<Quota> mQuota;
};

template <typename PromiseType, typename ResolverType, bool MoveOnly>
class PromiseResolveOrRejectCallback
    : public PromiseResolveOrRejectCallbackBase<PromiseType, ResolverType,
                                                MoveOnly> {};

template <typename PromiseType, typename ResolverType>
class PromiseResolveOrRejectCallback<PromiseType, ResolverType, true>
    : public PromiseResolveOrRejectCallbackBase<PromiseType, ResolverType,
                                                true> {
  using Base =
      PromiseResolveOrRejectCallbackBase<PromiseType, ResolverType, true>;

  using Base::CanSend;
  using Base::mResolver;

 public:
  PromiseResolveOrRejectCallback(RefPtr<Quota> aQuota, ResolverType&& aResolver)
      : Base(std::move(aQuota), std::move(aResolver)) {}

  void operator()(typename PromiseType::ResolveOrRejectValue&& aValue) {
    if (!CanSend()) {
      return;
    }
    if (aValue.IsResolve()) {
      mResolver(std::move(aValue.ResolveValue()));
    } else {
      mResolver(aValue.RejectValue());
    }
  }
};

template <typename PromiseType, typename ResolverType>
class PromiseResolveOrRejectCallback<PromiseType, ResolverType, false>
    : public PromiseResolveOrRejectCallbackBase<PromiseType, ResolverType,
                                                false> {
  using Base =
      PromiseResolveOrRejectCallbackBase<PromiseType, ResolverType, false>;

  using Base::CanSend;
  using Base::mResolver;

 public:
  PromiseResolveOrRejectCallback(RefPtr<Quota> aQuota, ResolverType&& aResolver)
      : Base(std::move(aQuota), std::move(aResolver)) {}

  void operator()(const typename PromiseType::ResolveOrRejectValue& aValue) {
    if (!CanSend()) {
      return;
    }

    if (aValue.IsResolve()) {
      mResolver(aValue.ResolveValue());
    } else {
      mResolver(aValue.RejectValue());
    }
  }
};

using BoolPromiseResolveOrRejectCallback =
    PromiseResolveOrRejectCallback<BoolPromise, BoolResponseResolver, false>;
using UInt64PromiseResolveOrRejectCallback =
    PromiseResolveOrRejectCallback<UInt64Promise, UInt64ResponseResolver,
                                   false>;
using CStringArrayPromiseResolveOrRejectCallback =
    PromiseResolveOrRejectCallback<CStringArrayPromise,
                                   CStringArrayResponseResolver, true>;
using OriginUsageMetadataArrayPromiseResolveOrRejectCallback =
    PromiseResolveOrRejectCallback<OriginUsageMetadataArrayPromise,
                                   OriginUsageMetadataArrayResponseResolver,
                                   true>;
using UsageInfoPromiseResolveOrRejectCallback =
    PromiseResolveOrRejectCallback<UsageInfoPromise, UsageInfoResponseResolver,
                                   false>;

}  // namespace

already_AddRefed<PQuotaParent> AllocPQuotaParent() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  auto actor = MakeRefPtr<Quota>();

  return actor.forget();
}

Quota::Quota()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
}

Quota::~Quota() { MOZ_ASSERT(mActorDestroyed); }

bool Quota::TrustParams() const {
#ifdef DEBUG
  // Never trust parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  return trustParams;
}

bool Quota::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TStorageNameParams:
      break;

    case RequestParams::TGetFullOriginMetadataParams: {
      const GetFullOriginMetadataParams& params =
          aParams.get_GetFullOriginMetadataParams();
      if (NS_WARN_IF(!IsBestEffortPersistenceType(params.persistenceType()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(!IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TListOriginsParams:
      break;

    case RequestParams::TPersistedParams: {
      const PersistedParams& params = aParams.get_PersistedParams();

      if (NS_WARN_IF(!IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TPersistParams: {
      const PersistParams& params = aParams.get_PersistParams();

      if (NS_WARN_IF(!IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TEstimateParams: {
      const EstimateParams& params = aParams.get_EstimateParams();

      if (NS_WARN_IF(!IsPrincipalInfoValid(params.principalInfo()))) {
        MOZ_CRASH_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void Quota::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif
}

PQuotaRequestParent* Quota::AllocPQuotaRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  if (!TrustParams() && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    MOZ_CRASH_UNLESS_FUZZING();
    return nullptr;
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(), nullptr);

  auto actor = [&]() -> RefPtr<QuotaRequestBase> {
    switch (aParams.type()) {
      case RequestParams::TStorageNameParams:
        return CreateStorageNameOp(quotaManager);

      case RequestParams::TGetFullOriginMetadataParams:
        return CreateGetFullOriginMetadataOp(
            quotaManager, aParams.get_GetFullOriginMetadataParams());

      case RequestParams::TPersistedParams:
        return CreatePersistedOp(quotaManager, aParams);

      case RequestParams::TPersistParams:
        return CreatePersistOp(quotaManager, aParams);

      case RequestParams::TEstimateParams:
        return CreateEstimateOp(quotaManager, aParams.get_EstimateParams());

      case RequestParams::TListOriginsParams:
        return CreateListOriginsOp(quotaManager);

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  quotaManager->RegisterNormalOriginOp(*actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaRequestConstructor(
    PQuotaRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  auto* op = static_cast<QuotaRequestBase*>(aActor);

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaRequestBase> actor =
      dont_AddRef(static_cast<QuotaRequestBase*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Quota::RecvStorageInitialized(
    StorageInitializedResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->StorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvPersistentStorageInitialized(
    TemporaryStorageInitializedResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->PersistentStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvTemporaryStorageInitialized(
    TemporaryStorageInitializedResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->TemporaryStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvTemporaryGroupInitialized(
    const PrincipalInfo& aPrincipalInfo,
    TemporaryOriginInitializedResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager->TemporaryGroupInitialized(aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvPersistentOriginInitialized(
    const PrincipalInfo& aPrincipalInfo,
    PersistentOriginInitializedResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      GetInfoFromValidatedPrincipalInfo(*quotaManager, aPrincipalInfo),
      ResolveBoolResponseAndReturn(aResolve));

  quotaManager
      ->PersistentOriginInitialized(OriginMetadata{std::move(principalMetadata),
                                                   PERSISTENCE_TYPE_PERSISTENT})
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvTemporaryOriginInitialized(
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    TemporaryOriginInitializedResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(aPersistenceType)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      GetInfoFromValidatedPrincipalInfo(*quotaManager, aPrincipalInfo),
      ResolveBoolResponseAndReturn(aResolve));

  quotaManager->TemporaryOriginInitialized(aPersistenceType, principalMetadata)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeStorage(
    InitializeStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->InitializeStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializePersistentStorage(
    InitializeStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->InitializePersistentStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryGroup(
    const PrincipalInfo& aPrincipalInfo,
    InitializeTemporaryOriginResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager->InitializeTemporaryGroup(aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializePersistentOrigin(
    const PrincipalInfo& aPrincipalInfo,
    InitializePersistentOriginResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      GetInfoFromValidatedPrincipalInfo(*quotaManager, aPrincipalInfo),
      ResolveBoolResponseAndReturn(aResolve));

  quotaManager
      ->InitializePersistentOrigin(OriginMetadata{std::move(principalMetadata),
                                                  PERSISTENCE_TYPE_PERSISTENT})
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryOrigin(
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const bool& aCreateIfNonExistent,
    InitializeTemporaryOriginResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(aPersistenceType)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  QM_TRY_UNWRAP(
      PrincipalMetadata principalMetadata,
      GetInfoFromValidatedPrincipalInfo(*quotaManager, aPrincipalInfo),
      ResolveBoolResponseAndReturn(aResolve));

  quotaManager
      ->InitializeTemporaryOrigin(aPersistenceType, principalMetadata,
                                  aCreateIfNonExistent)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializePersistentClient(
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    InitializeTemporaryClientResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager->InitializePersistentClient(aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryClient(
    const PersistenceType& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    InitializeTemporaryClientResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(aPersistenceType)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolve));

  quotaManager
      ->InitializeTemporaryClient(aPersistenceType, aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvInitializeTemporaryStorage(
    InitializeTemporaryStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->InitializeTemporaryStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvGetUsage(
    const bool& aGetAll,
    ManagedEndpoint<PQuotaUsageRequestParent>&& aParentEndpoint,
    GetUsageResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveOriginUsageMetadataArrayResponseAndReturn(aResolve));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveOriginUsageMetadataArrayResponseAndReturn(aResolve));

  auto parentActor = MakeRefPtr<QuotaUsageRequestParent>();

  auto cancelPromise = parentActor->OnCancel();

  QM_TRY(MOZ_TO_RESULT(BindPQuotaUsageRequestEndpoint(
             std::move(aParentEndpoint), parentActor)),
         ResolveOriginUsageMetadataArrayResponseAndReturn(aResolve));

  quotaManager->GetUsage(aGetAll, std::move(cancelPromise))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [parentActor](
              OriginUsageMetadataArrayPromise::ResolveOrRejectValue&& aValue) {
            parentActor->Destroy();

            return OriginUsageMetadataArrayPromise::CreateAndResolveOrReject(
                std::move(aValue), __func__);
          })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             OriginUsageMetadataArrayPromiseResolveOrRejectCallback(
                 this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvGetOriginUsage(
    const PrincipalInfo& aPrincipalInfo,
    ManagedEndpoint<PQuotaUsageRequestParent>&& aParentEndpoint,
    GetOriginUsageResolver&& aResolve) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveUsageInfoResponseAndReturn(aResolve));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveUsageInfoResponseAndReturn(aResolve));

  auto parentActor = MakeRefPtr<QuotaUsageRequestParent>();

  auto cancelPromise = parentActor->OnCancel();

  QM_TRY(MOZ_TO_RESULT(BindPQuotaUsageRequestEndpoint(
             std::move(aParentEndpoint), parentActor)),
         ResolveUsageInfoResponseAndReturn(aResolve));

  quotaManager->GetOriginUsage(aPrincipalInfo, std::move(cancelPromise))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [parentActor](const UsageInfoPromise::ResolveOrRejectValue& aValue) {
            parentActor->Destroy();

            return UsageInfoPromise::CreateAndResolveOrReject(aValue, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          UsageInfoPromiseResolveOrRejectCallback(this, std::move(aResolve)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvGetCachedOriginUsage(
    const PrincipalInfo& aPrincipalInfo,
    GetCachedOriginUsageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveUInt64ResponseAndReturn(aResolver));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveUInt64ResponseAndReturn(aResolver));

  quotaManager->GetCachedOriginUsage(aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             UInt64PromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvListCachedOrigins(
    ListCachedOriginsResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveCStringArrayResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveCStringArrayResponseAndReturn(aResolver));

  quotaManager->ListCachedOrigins()->Then(
      GetCurrentSerialEventTarget(), __func__,
      CStringArrayPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOrigin(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    ClearStoragesForOriginResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStoragesForOrigin(aPersistenceType, aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForClient(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    ClearStoragesForClientResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager
      ->ClearStoragesForClient(aPersistenceType, aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOriginPrefix(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    ClearStoragesForOriginResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStoragesForOriginPrefix(aPersistenceType, aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForOriginAttributesPattern(
    const OriginAttributesPattern& aPattern,
    ClearStoragesForOriginAttributesPatternResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(!BackgroundParent::IsOtherProcessActor(Manager())),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStoragesForOriginAttributesPattern(aPattern)->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStoragesForPrivateBrowsing(
    ClearStoragesForPrivateBrowsingResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    QM_TRY(MOZ_TO_RESULT(!BackgroundParent::IsOtherProcessActor(Manager())),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearPrivateRepository()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvClearStorage(
    ShutdownStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ClearStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvShutdownStoragesForOrigin(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo,
    ShutdownStoragesForOriginResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ShutdownStoragesForOrigin(aPersistenceType, aPrincipalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvShutdownStoragesForClient(
    const Maybe<PersistenceType>& aPersistenceType,
    const PrincipalInfo& aPrincipalInfo, const Type& aClientType,
    ShutdownStoragesForClientResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  if (!TrustParams()) {
    if (aPersistenceType) {
      QM_TRY(MOZ_TO_RESULT(IsValidPersistenceType(*aPersistenceType)),
             QM_CUF_AND_IPC_FAIL(this));
    }

    QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(aPrincipalInfo)),
           QM_CUF_AND_IPC_FAIL(this));

    QM_TRY(MOZ_TO_RESULT(Client::IsValidType(aClientType)),
           QM_CUF_AND_IPC_FAIL(this));
  }

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager
      ->ShutdownStoragesForClient(aPersistenceType, aPrincipalInfo, aClientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvShutdownStorage(
    ShutdownStorageResolver&& aResolver) {
  AssertIsOnBackgroundThread();

  QM_TRY(MOZ_TO_RESULT(!QuotaManager::IsShuttingDown()),
         ResolveBoolResponseAndReturn(aResolver));

  QM_TRY_UNWRAP(const NotNull<RefPtr<QuotaManager>> quotaManager,
                QuotaManager::GetOrCreate(),
                ResolveBoolResponseAndReturn(aResolver));

  quotaManager->ShutdownStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolPromiseResolveOrRejectCallback(this, std::move(aResolver)));

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStartIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QM_TRY(QuotaManager::EnsureCreated(), IPC_OK());

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->StartIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStopIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->StopIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvAbortOperationsForProcess(
    const ContentParentId& aContentParentId) {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    MOZ_CRASH_UNLESS_FUZZING();
    return IPC_FAIL(this, "Wrong actor");
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->AbortOperationsForProcess(aContentParentId);

  return IPC_OK();
}

}  // namespace mozilla::dom::quota
