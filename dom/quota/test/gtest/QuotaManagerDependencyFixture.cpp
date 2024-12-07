/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerDependencyFixture.h"

#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"
#include "nsIVariant.h"
#include "nsScriptSecurityManager.h"

namespace mozilla::dom::quota::test {

namespace {

class RequestResolver final : public nsIQuotaCallback {
 public:
  RequestResolver() : mDone(false) {}

  bool IsDone() const { return mDone; }

  NS_DECL_ISUPPORTS

  NS_IMETHOD OnComplete(nsIQuotaRequest* aRequest) override {
    mDone = true;

    return NS_OK;
  }

 private:
  ~RequestResolver() = default;

  bool mDone;
};

void CreateContentPrincipalInfo(const nsACString& aOrigin,
                                mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(aOrigin);
  QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

  mozilla::ipc::PrincipalInfo principalInfo;
  QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &aPrincipalInfo)),
         QM_TEST_FAIL);
}

}  // namespace

NS_IMPL_ISUPPORTS(RequestResolver, nsIQuotaCallback)

// static
void QuotaManagerDependencyFixture::InitializeFixture() {
  // Some QuotaManagerService methods fail if the testing pref is not set.
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  prefs->SetBoolPref("dom.quotaManager.testing", true);

  // The first initialization of storage service must be done on the main
  // thread.
  nsCOMPtr<mozIStorageService> storageService =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  ASSERT_TRUE(storageService);

  nsIObserver* observer = QuotaManager::GetObserver();
  ASSERT_TRUE(observer);

  nsresult rv = observer->Observe(nullptr, "profile-do-change", nullptr);
  ASSERT_NS_SUCCEEDED(rv);

  // Force creation of the quota manager.
  ASSERT_NO_FATAL_FAILURE(EnsureQuotaManager());

  QuotaManager* quotaManager = QuotaManager::Get();
  ASSERT_TRUE(quotaManager);

  ASSERT_TRUE(quotaManager->OwningThread());

  sBackgroundTarget = quotaManager->OwningThread();
}

// static
void QuotaManagerDependencyFixture::ShutdownFixture() {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  prefs->SetBoolPref("dom.quotaManager.testing", false);

  nsIObserver* observer = QuotaManager::GetObserver();
  ASSERT_TRUE(observer);

  nsresult rv = observer->Observe(nullptr, "profile-before-change-qm", nullptr);
  ASSERT_NS_SUCCEEDED(rv);

  PerformOnBackgroundThread([]() { QuotaManager::Reset(); });

  sBackgroundTarget = nullptr;
}

// static
void QuotaManagerDependencyFixture::InitializeStorage() {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->InitializeStorage());
  });
}

// static
void QuotaManagerDependencyFixture::StorageInitialized(bool* aResult) {
  ASSERT_TRUE(aResult);

  PerformOnBackgroundThread([aResult]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    auto value = Await(quotaManager->StorageInitialized());
    if (value.IsResolve()) {
      *aResult = value.ResolveValue();
    } else {
      *aResult = false;
    }
  });
}

// static
void QuotaManagerDependencyFixture::AssertStorageInitialized() {
  bool result;
  ASSERT_NO_FATAL_FAILURE(StorageInitialized(&result));
  ASSERT_TRUE(result);
}

// static
void QuotaManagerDependencyFixture::AssertStorageNotInitialized() {
  bool result;
  ASSERT_NO_FATAL_FAILURE(StorageInitialized(&result));
  ASSERT_FALSE(result);
}

// static
void QuotaManagerDependencyFixture::ClearStorage() {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->ClearStorage());
  });
}

// static
void QuotaManagerDependencyFixture::ShutdownStorage() {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->ShutdownStorage());
  });
}

// static
void QuotaManagerDependencyFixture::InitializeTemporaryStorage() {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->InitializeTemporaryStorage());
  });
}

// static
void QuotaManagerDependencyFixture::TemporaryStorageInitialized(bool* aResult) {
  ASSERT_TRUE(aResult);

  PerformOnBackgroundThread([aResult]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    auto value = Await(quotaManager->TemporaryStorageInitialized());
    if (value.IsResolve()) {
      *aResult = value.ResolveValue();
    } else {
      *aResult = false;
    }
  });
}

// static
void QuotaManagerDependencyFixture::AssertTemporaryStorageInitialized() {
  bool result;
  ASSERT_NO_FATAL_FAILURE(TemporaryStorageInitialized(&result));
  ASSERT_TRUE(result);
}

// static
void QuotaManagerDependencyFixture::AssertTemporaryStorageNotInitialized() {
  bool result;
  ASSERT_NO_FATAL_FAILURE(TemporaryStorageInitialized(&result));
  ASSERT_FALSE(result);
}

// static
void QuotaManagerDependencyFixture::ShutdownTemporaryStorage() {
  // TODO: It would be nice to have a dedicated operation for shutting down
  // temporary storage.
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
}

// static
void QuotaManagerDependencyFixture::InitializeTemporaryOrigin(
    const OriginMetadata& aOriginMetadata, bool aCreateIfNonExistent) {
  PerformOnBackgroundThread([aOriginMetadata, aCreateIfNonExistent]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->InitializeTemporaryOrigin(
        aOriginMetadata.mPersistenceType, aOriginMetadata,
        aCreateIfNonExistent));
  });
}

// static
void QuotaManagerDependencyFixture::TemporaryOriginInitialized(
    const OriginMetadata& aOriginMetadata, bool* aResult) {
  ASSERT_TRUE(aResult);

  PerformOnBackgroundThread([aOriginMetadata, aResult]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    auto value = Await(quotaManager->TemporaryOriginInitialized(
        aOriginMetadata.mPersistenceType, aOriginMetadata));
    if (value.IsResolve()) {
      *aResult = value.ResolveValue();
    } else {
      *aResult = false;
    }
  });
}

// static
void QuotaManagerDependencyFixture::AssertTemporaryOriginInitialized(
    const OriginMetadata& aOriginMetadata) {
  bool result;
  ASSERT_NO_FATAL_FAILURE(TemporaryOriginInitialized(aOriginMetadata, &result));
  ASSERT_TRUE(result);
}

// static
void QuotaManagerDependencyFixture::AssertTemporaryOriginNotInitialized(
    const OriginMetadata& aOriginMetadata) {
  bool result;
  ASSERT_NO_FATAL_FAILURE(TemporaryOriginInitialized(aOriginMetadata, &result));
  ASSERT_FALSE(result);
}

// static
void QuotaManagerDependencyFixture::GetOriginUsage(
    const OriginMetadata& aOriginMetadata, UsageInfo* aResult) {
  ASSERT_TRUE(aResult);

  mozilla::ipc::PrincipalInfo principalInfo;
  ASSERT_NO_FATAL_FAILURE(
      CreateContentPrincipalInfo(aOriginMetadata.mOrigin, principalInfo));

  PerformOnBackgroundThread(
      [aResult, principalInfo = std::move(principalInfo)]() {
        QuotaManager* quotaManager = QuotaManager::Get();
        ASSERT_TRUE(quotaManager);

        auto value = Await(quotaManager->GetOriginUsage(principalInfo));
        if (value.IsResolve()) {
          *aResult = value.ResolveValue();
        } else {
          *aResult = UsageInfo();
        }
      });
}

// static
void QuotaManagerDependencyFixture::GetCachedOriginUsage(
    const OriginMetadata& aOriginMetadata, UsageInfo* aResult) {
  ASSERT_TRUE(aResult);

  mozilla::ipc::PrincipalInfo principalInfo;
  ASSERT_NO_FATAL_FAILURE(
      CreateContentPrincipalInfo(aOriginMetadata.mOrigin, principalInfo));

  PerformOnBackgroundThread(
      [aResult, principalInfo = std::move(principalInfo)]() {
        QuotaManager* quotaManager = QuotaManager::Get();
        ASSERT_TRUE(quotaManager);

        auto value = Await(quotaManager->GetCachedOriginUsage(principalInfo));
        if (value.IsResolve()) {
          *aResult = UsageInfo(DatabaseUsageType(Some(value.ResolveValue())));
        } else {
          *aResult = UsageInfo();
        }
      });
}

// static
void QuotaManagerDependencyFixture::ClearStoragesForOrigin(
    const OriginMetadata& aOriginMetadata) {
  mozilla::ipc::PrincipalInfo principalInfo;
  ASSERT_NO_FATAL_FAILURE(
      CreateContentPrincipalInfo(aOriginMetadata.mOrigin, principalInfo));

  PerformOnBackgroundThread([principalInfo = std::move(principalInfo)]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->ClearStoragesForOrigin(/* aPersistenceType */ Nothing(),
                                               principalInfo));
  });
}

// static
void QuotaManagerDependencyFixture::InitializeTemporaryClient(
    const ClientMetadata& aClientMetadata) {
  mozilla::ipc::PrincipalInfo principalInfo;
  ASSERT_NO_FATAL_FAILURE(
      CreateContentPrincipalInfo(aClientMetadata.mOrigin, principalInfo));

  PerformOnBackgroundThread([persistenceType = aClientMetadata.mPersistenceType,
                             principalInfo = std::move(principalInfo),
                             clientType = aClientMetadata.mClientType]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    Await(quotaManager->InitializeTemporaryClient(persistenceType,
                                                  principalInfo, clientType));
  });
}

// static
PrincipalMetadata QuotaManagerDependencyFixture::GetTestPrincipalMetadata() {
  return {""_ns, "example.com"_ns, "http://example.com"_ns,
          "http://example.com"_ns,
          /* aIsPrivate */ false};
}

// static
OriginMetadata
QuotaManagerDependencyFixture::GetTestPersistentOriginMetadata() {
  return {GetTestPrincipalMetadata(), PERSISTENCE_TYPE_PERSISTENT};
}

// static
OriginMetadata QuotaManagerDependencyFixture::GetTestOriginMetadata() {
  return {GetTestPrincipalMetadata(), PERSISTENCE_TYPE_DEFAULT};
}

// static
ClientMetadata QuotaManagerDependencyFixture::GetTestClientMetadata() {
  return {GetTestOriginMetadata(), Client::SDB};
}

// static
PrincipalMetadata
QuotaManagerDependencyFixture::GetOtherTestPrincipalMetadata() {
  return {""_ns, "other-example.com"_ns, "http://other-example.com"_ns,
          "http://other-example.com"_ns,
          /* aIsPrivate */ false};
}

// static
OriginMetadata QuotaManagerDependencyFixture::GetOtherTestOriginMetadata() {
  return {GetOtherTestPrincipalMetadata(), PERSISTENCE_TYPE_DEFAULT};
}

// static
ClientMetadata QuotaManagerDependencyFixture::GetOtherTestClientMetadata() {
  return {GetOtherTestOriginMetadata(), Client::SDB};
}

// static
void QuotaManagerDependencyFixture::EnsureQuotaManager() {
  // This is needed to satisfy the IsCallerChrome check in
  // QuotaManagerService::StorageName. In more detail, accessing the Subject
  // Principal without an AutoJSAPI on the stack is forbidden.
  AutoJSAPI jsapi;

  bool ok = jsapi.Init(xpc::PrivilegedJunkScope());
  ASSERT_TRUE(ok);

  nsCOMPtr<nsIQuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  ASSERT_TRUE(qms);

  // In theory, any nsIQuotaManagerService method which ensures quota manager
  // on the PBackground thread, could be called here. `StorageName` was chosen
  // because it doesn't need to do any directory locking or IO.
  // XXX Consider adding a dedicated nsIQuotaManagerService method for this.
  nsCOMPtr<nsIQuotaRequest> request;
  nsresult rv = qms->StorageName(getter_AddRefs(request));
  ASSERT_NS_SUCCEEDED(rv);

  RefPtr<RequestResolver> resolver = new RequestResolver();

  rv = request->SetCallback(resolver);
  ASSERT_NS_SUCCEEDED(rv);

  SpinEventLoopUntil("Promise is fulfilled"_ns,
                     [&resolver]() { return resolver->IsDone(); });
}

MOZ_RUNINIT nsCOMPtr<nsISerialEventTarget>
    QuotaManagerDependencyFixture::sBackgroundTarget;

}  // namespace mozilla::dom::quota::test
