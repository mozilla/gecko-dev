/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerService.h"

// Local includes
#include "ActorsChild.h"
#include "Client.h"
#include "QuotaManager.h"
#include "QuotaRequests.h"
#include "QuotaResults.h"

// Global includes
#include <cstdint>
#include <cstring>
#include <utility>
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/quota/PrincipalUtils.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/QuotaUsageRequestChild.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/fallible.h"
#include "mozilla/hal_sandbox/PHal.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIUserIdleService.h"
#include "nsServiceManagerUtils.h"
#include "nsStringFwd.h"
#include "nsVariant.h"
#include "nsXULAppAPI.h"
#include "nscore.h"

#define PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID "profile-before-change-qm"

namespace mozilla::dom::quota {

using namespace mozilla::ipc;

namespace {

const char kIdleServiceContractId[] = "@mozilla.org/widget/useridleservice;1";

// The number of seconds we will wait after receiving the idle-daily
// notification before beginning maintenance.
const uint32_t kIdleObserverTimeSec = 1;

mozilla::StaticRefPtr<QuotaManagerService> gQuotaManagerService;

mozilla::Atomic<bool> gInitialized(false);
mozilla::Atomic<bool> gClosed(false);

nsresult CheckedPrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                         PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(aPrincipal);

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!IsPrincipalInfoValid(aPrincipalInfo))) {
    return NS_ERROR_FAILURE;
  }

  if (aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

template <typename ResponseType>
struct ResponseTypeTraits;

template <>
struct ResponseTypeTraits<BoolResponse> {
  static constexpr auto kType = BoolResponse::Tbool;

  static RefPtr<nsVariant> CreateVariant(const BoolResponse& aResponse) {
    auto variant = MakeRefPtr<nsVariant>();
    variant->SetAsBool(aResponse.get_bool());
    return variant;
  }
};

template <>
struct ResponseTypeTraits<UInt64Response> {
  static constexpr auto kType = UInt64Response::Tuint64_t;

  static RefPtr<nsVariant> CreateVariant(const UInt64Response& aResponse) {
    RefPtr<nsVariant> variant = new nsVariant();
    variant->SetAsUint64(aResponse.get_uint64_t());
    return variant;
  }
};

template <>
struct ResponseTypeTraits<CStringArrayResponse> {
  static constexpr auto kType = CStringArrayResponse::TArrayOfnsCString;

  static RefPtr<nsVariant> CreateVariant(
      const CStringArrayResponse& aResponse) {
    const CStringArray& strings = aResponse.get_ArrayOfnsCString();

    auto variant = MakeRefPtr<nsVariant>();

    if (strings.IsEmpty()) {
      MOZ_ALWAYS_SUCCEEDS(variant->SetAsEmptyArray());
    } else {
      nsTArray<const char*> stringPointers(strings.Length());

      std::transform(strings.cbegin(), strings.cend(),
                     MakeBackInserter(stringPointers),
                     std::mem_fn(&nsCString::get));

      QM_TRY(MOZ_TO_RESULT(variant->SetAsArray(
                 nsIDataType::VTYPE_CHAR_STR, /* aIID */ nullptr,
                 stringPointers.Length(), stringPointers.Elements())),
             nullptr);
    }

    return variant;
  }
};

template <>
struct ResponseTypeTraits<OriginUsageMetadataArrayResponse> {
  static constexpr auto kType =
      OriginUsageMetadataArrayResponse::TOriginUsageMetadataArray;

  static RefPtr<nsVariant> CreateVariant(
      const OriginUsageMetadataArrayResponse& aResponse) {
    const OriginUsageMetadataArray& originUsages =
        aResponse.get_OriginUsageMetadataArray();

    auto variant = MakeRefPtr<nsVariant>();

    if (originUsages.IsEmpty()) {
      variant->SetAsEmptyArray();
    } else {
      nsTArray<RefPtr<UsageResult>> usageResults(originUsages.Length());

      for (const auto& originUsage : originUsages) {
        usageResults.AppendElement(MakeRefPtr<UsageResult>(
            originUsage.mOrigin, originUsage.mPersisted, originUsage.mUsage,
            originUsage.mLastAccessTime));
      }

      variant->SetAsArray(
          nsIDataType::VTYPE_INTERFACE_IS, &NS_GET_IID(nsIQuotaUsageResult),
          usageResults.Length(), static_cast<void*>(usageResults.Elements()));
    }

    return variant;
  }
};

template <>
struct ResponseTypeTraits<UsageInfoResponse> {
  static constexpr auto kType = UsageInfoResponse::TUsageInfo;

  static RefPtr<nsVariant> CreateVariant(const UsageInfoResponse& aResponse) {
    RefPtr<OriginUsageResult> result =
        new OriginUsageResult(aResponse.get_UsageInfo());

    auto variant = MakeRefPtr<nsVariant>();
    variant->SetAsInterface(NS_GET_IID(nsIQuotaOriginUsageResult), result);

    return variant;
  }
};

template <typename RequestType, typename PromiseType, typename ResponseType>
class ResponsePromiseResolveOrRejectCallback {
 public:
  explicit ResponsePromiseResolveOrRejectCallback(RefPtr<RequestType> aRequest)
      : mRequest(std::move(aRequest)) {}

  void operator()(const typename PromiseType::ResolveOrRejectValue& aValue) {
    if (aValue.IsResolve()) {
      const ResponseType& response = aValue.ResolveValue();

      switch (response.type()) {
        case ResponseType::Tnsresult:
          mRequest->SetError(response.get_nsresult());
          break;

        case ResponseTypeTraits<ResponseType>::kType: {
          RefPtr<nsVariant> variant =
              ResponseTypeTraits<ResponseType>::CreateVariant(response);

          if (variant) {
            mRequest->SetResult(variant);
          } else {
            mRequest->SetError(NS_ERROR_FAILURE);
          }
          break;
        }
        default:
          MOZ_CRASH("Unknown response type!");
      }

    } else {
      mRequest->SetError(NS_ERROR_FAILURE);
    }
  }

 private:
  RefPtr<RequestType> mRequest;
};

using BoolResponsePromiseResolveOrRejectCallback =
    ResponsePromiseResolveOrRejectCallback<Request, BoolResponsePromise,
                                           BoolResponse>;
using UInt64ResponsePromiseResolveOrRejectCallback =
    ResponsePromiseResolveOrRejectCallback<Request, UInt64ResponsePromise,
                                           UInt64Response>;
using CStringArrayResponsePromiseResolveOrRejectCallback =
    ResponsePromiseResolveOrRejectCallback<Request, CStringArrayResponsePromise,
                                           CStringArrayResponse>;
using OriginUsageMetadataArrayResponsePromiseResolveOrRejectCallback =
    ResponsePromiseResolveOrRejectCallback<
        UsageRequest, OriginUsageMetadataArrayResponsePromise,
        OriginUsageMetadataArrayResponse>;
using UsageInfoResponsePromiseResolveOrRejectCallback =
    ResponsePromiseResolveOrRejectCallback<
        UsageRequest, UsageInfoResponsePromise, UsageInfoResponse>;

}  // namespace

class QuotaManagerService::PendingRequestInfo {
 protected:
  RefPtr<RequestBase> mRequest;

 public:
  explicit PendingRequestInfo(RequestBase* aRequest) : mRequest(aRequest) {}

  virtual ~PendingRequestInfo() = default;

  RequestBase* GetRequest() const { return mRequest; }

  virtual nsresult InitiateRequest(QuotaChild* aActor) = 0;
};

class QuotaManagerService::RequestInfo : public PendingRequestInfo {
  RequestParams mParams;

 public:
  RequestInfo(Request* aRequest, const RequestParams& aParams)
      : PendingRequestInfo(aRequest), mParams(aParams) {
    MOZ_ASSERT(aRequest);
    MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  }

  virtual nsresult InitiateRequest(QuotaChild* aActor) override;
};

class QuotaManagerService::IdleMaintenanceInfo : public PendingRequestInfo {
  const bool mStart;

 public:
  explicit IdleMaintenanceInfo(bool aStart)
      : PendingRequestInfo(nullptr), mStart(aStart) {}

  virtual nsresult InitiateRequest(QuotaChild* aActor) override;
};

QuotaManagerService::QuotaManagerService()
    : mBackgroundActor(nullptr),
      mBackgroundActorFailed(false),
      mIdleObserverRegistered(false) {
  MOZ_ASSERT(NS_IsMainThread());
}

QuotaManagerService::~QuotaManagerService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIdleObserverRegistered);
}

// static
QuotaManagerService* QuotaManagerService::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());

  if (gClosed) {
    MOZ_ASSERT(false, "Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gQuotaManagerService) {
    RefPtr<QuotaManagerService> instance(new QuotaManagerService());

    nsresult rv = instance->Init();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    if (gInitialized.exchange(true)) {
      MOZ_ASSERT(false, "Initialized more than once?!");
    }

    gQuotaManagerService = instance;

    ClearOnShutdown(&gQuotaManagerService);
  }

  return gQuotaManagerService;
}

// static
QuotaManagerService* QuotaManagerService::Get() {
  // Does not return an owning reference.
  return gQuotaManagerService;
}

// static
already_AddRefed<QuotaManagerService> QuotaManagerService::FactoryCreate() {
  RefPtr<QuotaManagerService> quotaManagerService = GetOrCreate();
  return quotaManagerService.forget();
}

void QuotaManagerService::ClearBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  mBackgroundActor = nullptr;
}

void QuotaManagerService::AbortOperationsForProcess(
    ContentParentId aContentParentId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = EnsureBackgroundActor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (NS_WARN_IF(
          !mBackgroundActor->SendAbortOperationsForProcess(aContentParentId))) {
    return;
  }
}

nsresult QuotaManagerService::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = observerService->AddObserver(
        this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void QuotaManagerService::Destroy() {
  // Setting the closed flag prevents the service from being recreated.
  // Don't set it though if there's no real instance created.
  if (gInitialized && gClosed.exchange(true)) {
    MOZ_ASSERT(false, "Shutdown more than once?!");
  }

  delete this;
}

nsresult QuotaManagerService::EnsureBackgroundActor() {
  MOZ_ASSERT(NS_IsMainThread());

  // Nothing can be done here if we have previously failed to create a
  // background actor.
  if (mBackgroundActorFailed) {
    return NS_ERROR_FAILURE;
  }

  if (!mBackgroundActor) {
    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      mBackgroundActorFailed = true;
      return NS_ERROR_FAILURE;
    }

    {
      RefPtr<QuotaChild> actor = new QuotaChild(this);

      mBackgroundActor = static_cast<QuotaChild*>(
          backgroundActor->SendPQuotaConstructor(actor));
    }
  }

  if (!mBackgroundActor) {
    mBackgroundActorFailed = true;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult QuotaManagerService::InitiateRequest(PendingRequestInfo& aInfo) {
  nsresult rv = EnsureBackgroundActor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aInfo.InitiateRequest(mBackgroundActor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void QuotaManagerService::PerformIdleMaintenance() {
  using namespace mozilla::hal;

  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // If we're running on battery power then skip all idle maintenance since we
  // would otherwise be doing lots of disk I/O.
  BatteryInformation batteryInfo;

#ifdef MOZ_WIDGET_ANDROID
  // Android XPCShell doesn't load the AndroidBridge that is needed to make
  // GetCurrentBatteryInformation work...
  if (!QuotaManager::IsRunningXPCShellTests())
#endif
  {
    // In order to give the correct battery level, hal must have registered
    // battery observers.
    RegisterBatteryObserver(this);
    GetCurrentBatteryInformation(&batteryInfo);
    UnregisterBatteryObserver(this);
  }

  // If we're running XPCShell because we always want to be able to test this
  // code so pretend that we're always charging.
  if (QuotaManager::IsRunningXPCShellTests()) {
    batteryInfo.level() = 100;
    batteryInfo.charging() = true;
  }

  if (NS_WARN_IF(!batteryInfo.charging())) {
    return;
  }

  if (QuotaManager::IsRunningXPCShellTests()) {
    // We don't want user activity to impact this code if we're running tests.
    Unused << Observe(nullptr, OBSERVER_TOPIC_IDLE, nullptr);
  } else if (!mIdleObserverRegistered) {
    nsCOMPtr<nsIUserIdleService> idleService =
        do_GetService(kIdleServiceContractId);
    MOZ_ASSERT(idleService);

    MOZ_ALWAYS_SUCCEEDS(
        idleService->AddIdleObserver(this, kIdleObserverTimeSec));

    mIdleObserverRegistered = true;
  }
}

void QuotaManagerService::RemoveIdleObserver() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mIdleObserverRegistered) {
    nsCOMPtr<nsIUserIdleService> idleService =
        do_GetService(kIdleServiceContractId);
    MOZ_ASSERT(idleService);

    // Ignore the return value of RemoveIdleObserver, it may fail if the
    // observer has already been unregistered during shutdown.
    Unused << idleService->RemoveIdleObserver(this, kIdleObserverTimeSec);

    mIdleObserverRegistered = false;
  }
}

NS_IMPL_ADDREF(QuotaManagerService)
NS_IMPL_RELEASE_WITH_DESTROY(QuotaManagerService, Destroy())
NS_IMPL_QUERY_INTERFACE(QuotaManagerService, nsIQuotaManagerService,
                        nsIQuotaManagerServiceInternal, nsIObserver)

NS_IMETHODIMP
QuotaManagerService::StorageName(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Request> request = new Request();

  StorageNameParams params;

  RequestInfo info(request, params);

  nsresult rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::StorageInitialized(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::PersistentStorageInitialized(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendPersistentStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::TemporaryStorageInitialized(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendTemporaryStorageInitialized()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::TemporaryGroupInitialized(nsIPrincipal* aPrincipal,
                                               nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendTemporaryGroupInitialized(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::PersistentOriginInitialized(nsIPrincipal* aPrincipal,
                                                 nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendPersistentOriginInitialized(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::TemporaryOriginInitialized(
    const nsACString& aPersistenceType, nsIPrincipal* aPrincipal,
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<PersistenceType, nsresult> {
        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        QM_TRY(
            MOZ_TO_RESULT(IsBestEffortPersistenceType(persistenceType.ref())),
            Err(NS_ERROR_INVALID_ARG));

        return persistenceType.ref();
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendTemporaryOriginInitialized(persistenceType, principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Init(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendInitializeStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializePersistentStorage(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendInitializePersistentStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitTemporaryStorage(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendInitializeTemporaryStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializeTemporaryGroup(nsIPrincipal* aPrincipal,
                                              nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendInitializeTemporaryGroup(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializePersistentOrigin(nsIPrincipal* aPrincipal,
                                                nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  auto request = MakeRefPtr<Request>();

  mBackgroundActor->SendInitializePersistentOrigin(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializeTemporaryOrigin(
    const nsACString& aPersistenceType, nsIPrincipal* aPrincipal,
    bool aCreateIfNonExistent, nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<PersistenceType, nsresult> {
        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        QM_TRY(
            MOZ_TO_RESULT(IsBestEffortPersistenceType(persistenceType.ref())),
            Err(NS_ERROR_INVALID_ARG));

        return persistenceType.ref();
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  auto request = MakeRefPtr<Request>();

  mBackgroundActor
      ->SendInitializeTemporaryOrigin(persistenceType, principalInfo,
                                      aCreateIfNonExistent)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializePersistentClient(nsIPrincipal* aPrincipal,
                                                const nsAString& aClientType,
                                                nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  QM_TRY_INSPECT(const auto& clientType,
                 ([&aClientType]() -> Result<Client::Type, nsresult> {
                   Client::Type clientType;
                   QM_TRY(MOZ_TO_RESULT(Client::TypeFromText(
                              aClientType, clientType, fallible)),
                          Err(NS_ERROR_INVALID_ARG));

                   return clientType;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendInitializePersistentClient(principalInfo, clientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::InitializeTemporaryClient(
    const nsACString& aPersistenceType, nsIPrincipal* aPrincipal,
    const nsAString& aClientType, nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(MOZ_TO_RESULT(StaticPrefs::dom_quotaManager_testing()),
         NS_ERROR_UNEXPECTED);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<PersistenceType, nsresult> {
        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        QM_TRY(
            MOZ_TO_RESULT(IsBestEffortPersistenceType(persistenceType.ref())),
            Err(NS_ERROR_INVALID_ARG));

        return persistenceType.ref();
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  QM_TRY_INSPECT(const auto& clientType,
                 ([&aClientType]() -> Result<Client::Type, nsresult> {
                   Client::Type clientType;
                   QM_TRY(MOZ_TO_RESULT(Client::TypeFromText(
                              aClientType, clientType, fallible)),
                          Err(NS_ERROR_INVALID_ARG));

                   return clientType;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendInitializeTemporaryClient(persistenceType, principalInfo,
                                      clientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetFullOriginMetadata(const nsACString& aPersistenceType,
                                           nsIPrincipal* aPrincipal,
                                           nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  QM_TRY(OkIf(StaticPrefs::dom_quotaManager_testing()), NS_ERROR_UNEXPECTED);

  const auto maybePersistenceType =
      PersistenceTypeFromString(aPersistenceType, fallible);
  QM_TRY(OkIf(maybePersistenceType.isSome()), NS_ERROR_INVALID_ARG);
  QM_TRY(OkIf(IsBestEffortPersistenceType(*maybePersistenceType)),
         NS_ERROR_INVALID_ARG);

  PrincipalInfo principalInfo;
  QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));
  QM_TRY(OkIf(IsPrincipalInfoValid(principalInfo)), NS_ERROR_INVALID_ARG);

  RefPtr<Request> request = new Request();

  GetFullOriginMetadataParams params;

  params.persistenceType() = *maybePersistenceType;
  params.principalInfo() = std::move(principalInfo);

  RequestInfo info(request, params);

  QM_TRY(MOZ_TO_RESULT(InitiateRequest(info)));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetUsage(nsIQuotaUsageCallback* aCallback, bool aGetAll,
                              nsIQuotaUsageRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<UsageRequest> request = new UsageRequest(aCallback);

  RefPtr<QuotaUsageRequestChild> usageRequestChild =
      new QuotaUsageRequestChild(request);

  ManagedEndpoint<PQuotaUsageRequestParent> usageRequestParentEndpoint =
      mBackgroundActor->OpenPQuotaUsageRequestEndpoint(usageRequestChild);
  QM_TRY(MOZ_TO_RESULT(usageRequestParentEndpoint.IsValid()));

  mBackgroundActor->SendGetUsage(aGetAll, std::move(usageRequestParentEndpoint))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             OriginUsageMetadataArrayResponsePromiseResolveOrRejectCallback(
                 request));

  request->SetBackgroundActor(usageRequestChild);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetUsageForPrincipal(nsIPrincipal* aPrincipal,
                                          nsIQuotaUsageCallback* aCallback,
                                          nsIQuotaUsageRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<UsageRequest> request = new UsageRequest(aPrincipal, aCallback);

  RefPtr<QuotaUsageRequestChild> usageRequestChild =
      new QuotaUsageRequestChild(request);

  ManagedEndpoint<PQuotaUsageRequestParent> usageRequestParentEndpoint =
      mBackgroundActor->OpenPQuotaUsageRequestEndpoint(usageRequestChild);
  QM_TRY(MOZ_TO_RESULT(usageRequestParentEndpoint.IsValid()));

  mBackgroundActor
      ->SendGetOriginUsage(principalInfo, std::move(usageRequestParentEndpoint))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             UsageInfoResponsePromiseResolveOrRejectCallback(request));

  request->SetBackgroundActor(usageRequestChild);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::GetCachedUsageForPrincipal(nsIPrincipal* aPrincipal,
                                                nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendGetCachedOriginUsage(principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             UInt64ResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Clear(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendClearStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForPrivateBrowsing(
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendClearStoragesForPrivateBrowsing()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForOriginAttributesPattern(
    const nsAString& aPattern, nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  OriginAttributesPattern pattern;
  MOZ_ALWAYS_TRUE(pattern.Init(aPattern));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendClearStoragesForOriginAttributesPattern(pattern)->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForPrincipal(
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<Maybe<PersistenceType>, nsresult> {
        if (aPersistenceType.IsVoid()) {
          return Maybe<PersistenceType>();
        }

        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        return persistenceType;
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendClearStoragesForOrigin(persistenceType, principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForClient(nsIPrincipal* aPrincipal,
                                            const nsAString& aClientType,
                                            const nsACString& aPersistenceType,
                                            nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<Maybe<PersistenceType>, nsresult> {
        if (aPersistenceType.IsVoid()) {
          return Maybe<PersistenceType>();
        }

        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        return persistenceType;
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  QM_TRY_INSPECT(const auto& clientType,
                 ([&aClientType]() -> Result<Client::Type, nsresult> {
                   Client::Type clientType;
                   QM_TRY(MOZ_TO_RESULT(Client::TypeFromText(
                              aClientType, clientType, fallible)),
                          Err(NS_ERROR_INVALID_ARG));

                   return clientType;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendClearStoragesForClient(persistenceType, principalInfo, clientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ClearStoragesForOriginPrefix(
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<Maybe<PersistenceType>, nsresult> {
        if (aPersistenceType.IsVoid()) {
          return Maybe<PersistenceType>();
        }

        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        return persistenceType;
      }()));

  QM_TRY_INSPECT(
      const auto& principalInfo,
      ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
        PrincipalInfo principalInfo;
        QM_TRY(MOZ_TO_RESULT(
            PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

        QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
               Err(NS_ERROR_INVALID_ARG));

        if (principalInfo.type() == PrincipalInfo::TContentPrincipalInfo) {
          nsCString suffix;
          principalInfo.get_ContentPrincipalInfo().attrs().CreateSuffix(suffix);

          QM_TRY(MOZ_TO_RESULT(suffix.IsEmpty()), Err(NS_ERROR_INVALID_ARG));
        }

        return principalInfo;
      }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendClearStoragesForOriginPrefix(persistenceType, principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Reset(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!StaticPrefs::dom_quotaManager_testing())) {
    return NS_ERROR_UNEXPECTED;
  }

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  RefPtr<Request> request = new Request();

  mBackgroundActor->SendShutdownStorage()->Then(
      GetCurrentSerialEventTarget(), __func__,
      BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ResetStoragesForPrincipal(
    nsIPrincipal* aPrincipal, const nsACString& aPersistenceType,
    nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<Maybe<PersistenceType>, nsresult> {
        if (aPersistenceType.IsVoid()) {
          return Maybe<PersistenceType>();
        }

        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        return persistenceType;
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendShutdownStoragesForOrigin(persistenceType, principalInfo)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ResetStoragesForClient(nsIPrincipal* aPrincipal,
                                            const nsAString& aClientType,
                                            const nsACString& aPersistenceType,
                                            nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  QM_TRY_INSPECT(
      const auto& persistenceType,
      ([&aPersistenceType]() -> Result<Maybe<PersistenceType>, nsresult> {
        if (aPersistenceType.IsVoid()) {
          return Maybe<PersistenceType>();
        }

        const auto persistenceType =
            PersistenceTypeFromString(aPersistenceType, fallible);
        QM_TRY(MOZ_TO_RESULT(persistenceType.isSome()),
               Err(NS_ERROR_INVALID_ARG));

        return persistenceType;
      }()));

  QM_TRY_INSPECT(const auto& principalInfo,
                 ([&aPrincipal]() -> Result<PrincipalInfo, nsresult> {
                   PrincipalInfo principalInfo;
                   QM_TRY(MOZ_TO_RESULT(
                       PrincipalToPrincipalInfo(aPrincipal, &principalInfo)));

                   QM_TRY(MOZ_TO_RESULT(IsPrincipalInfoValid(principalInfo)),
                          Err(NS_ERROR_INVALID_ARG));

                   return principalInfo;
                 }()));

  QM_TRY_INSPECT(const auto& clientType,
                 ([&aClientType]() -> Result<Client::Type, nsresult> {
                   Client::Type clientType;
                   QM_TRY(MOZ_TO_RESULT(Client::TypeFromText(
                              aClientType, clientType, fallible)),
                          Err(NS_ERROR_INVALID_ARG));

                   return clientType;
                 }()));

  RefPtr<Request> request = new Request();

  mBackgroundActor
      ->SendShutdownStoragesForClient(persistenceType, principalInfo,
                                      clientType)
      ->Then(GetCurrentSerialEventTarget(), __func__,
             BoolResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Persisted(nsIPrincipal* aPrincipal,
                               nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistedParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RequestInfo info(request, params);

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Persist(nsIPrincipal* aPrincipal,
                             nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Request> request = new Request(aPrincipal);

  PersistParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RequestInfo info(request, params);

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Estimate(nsIPrincipal* aPrincipal,
                              nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  RefPtr<Request> request = new Request(aPrincipal);

  EstimateParams params;

  nsresult rv =
      CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RequestInfo info(request, params);

  rv = InitiateRequest(info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ListOrigins(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  auto request = MakeRefPtr<Request>();

  mBackgroundActor->SendListOrigins()->Then(
      GetCurrentSerialEventTarget(), __func__,
      CStringArrayResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::ListCachedOrigins(nsIQuotaRequest** _retval) {
  MOZ_ASSERT(NS_IsMainThread());

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  auto request = MakeRefPtr<Request>();

  mBackgroundActor->SendListCachedOrigins()->Then(
      GetCurrentSerialEventTarget(), __func__,
      CStringArrayResponsePromiseResolveOrRejectCallback(request));

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::SetThumbnailPrivateIdentityId(
    uint32_t aThumbnailPrivateIdentityId) {
  MOZ_ASSERT(NS_IsMainThread());

  QM_TRY(MOZ_TO_RESULT(EnsureBackgroundActor()));

  mBackgroundActor->SendSetThumbnailPrivateIdentityId(
      aThumbnailPrivateIdentityId);

  return NS_OK;
}

NS_IMETHODIMP
QuotaManagerService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID)) {
    RemoveIdleObserver();
    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY)) {
    PerformIdleMaintenance();
    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    IdleMaintenanceInfo info(/* aStart */ true);

    nsresult rv = InitiateRequest(info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, OBSERVER_TOPIC_ACTIVE)) {
    RemoveIdleObserver();

    IdleMaintenanceInfo info(/* aStart */ false);

    nsresult rv = InitiateRequest(info);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Should never get here!");
  return NS_OK;
}

void QuotaManagerService::Notify(const hal::BatteryInformation& aBatteryInfo) {
  // This notification is received when battery data changes. We don't need to
  // deal with this notification.
}

nsresult QuotaManagerService::RequestInfo::InitiateRequest(QuotaChild* aActor) {
  MOZ_ASSERT(aActor);

  auto request = static_cast<Request*>(mRequest.get());

  auto actor = new QuotaRequestChild(request);

  if (!aActor->SendPQuotaRequestConstructor(actor, mParams)) {
    request->SetError(NS_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult QuotaManagerService::IdleMaintenanceInfo::InitiateRequest(
    QuotaChild* aActor) {
  MOZ_ASSERT(aActor);

  bool result;

  if (mStart) {
    result = aActor->SendStartIdleMaintenance();
  } else {
    result = aActor->SendStopIdleMaintenance();
  }

  if (!result) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla::dom::quota
