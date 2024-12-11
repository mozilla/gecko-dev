/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/dom/quota/ClientDirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLockInlines.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/PersistenceScope.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UniversalDirectoryLock.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsFmtString.h"
#include "QuotaManagerDependencyFixture.h"
#include "QuotaManagerTestHelpers.h"

namespace mozilla::dom::quota::test {

class TestQuotaManager : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }

  void TearDown() override {
    ASSERT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));
  }
};

TEST_F(TestQuotaManager, GetThumbnailPrivateIdentityId) {
  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    const bool known = quotaManager->IsThumbnailPrivateIdentityIdKnown();
    ASSERT_TRUE(known);

    const uint32_t id = quotaManager->GetThumbnailPrivateIdentityId();
    ASSERT_GT(id, 4u);
  });
}

class TestQuotaManagerAndShutdownFixture
    : public QuotaManagerDependencyFixture {
 public:
  void SetUp() override { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  void TearDown() override { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

// Test OpenStorageDirectory when an opening of the storage directory is
// already ongoing and storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, OpenStorageDirectory_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock;

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                           aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     [&aValue]() { ASSERT_TRUE(aValue.ResolveValue()); }();

                     directoryLock = std::move(aValue.ResolveValue());

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(quotaManager->IOThread(), __func__,
                   [](const BoolPromise::ResolveOrRejectValue& aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     []() {
                       QuotaManager* quotaManager = QuotaManager::Get();
                       ASSERT_TRUE(quotaManager);

                       ASSERT_TRUE(
                           quotaManager->IsStorageInitializedInternal());
                     }();

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     DropDirectoryLock(directoryLock);

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     RefPtr<UniversalDirectoryLock> directoryLock =
                         std::move(aValue.ResolveValue());
                     DropDirectoryLock(directoryLock);

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory is
// already ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       OpenStorageDirectory_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                           aValue) {
                     DropDirectoryLock(directoryLock);

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     RefPtr<UniversalDirectoryLock> directoryLock =
                         std::move(aValue.ResolveValue());
                     DropDirectoryLock(directoryLock);

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     RefPtr<UniversalDirectoryLock> directoryLock =
                         std::move(aValue.ResolveValue());
                     DropDirectoryLock(directoryLock);

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished.
TEST_F(TestQuotaManager, OpenStorageDirectory_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->OpenStorageDirectory(
          PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
          OriginScope::FromNull(), Nullable<Client::Type>(),
          /* aExclusive */ false));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<UniversalDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    {
      auto value = Await(quotaManager->OpenStorageDirectory(
          PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
          OriginScope::FromNull(), Nullable<Client::Type>(),
          /* aExclusive */ false));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<UniversalDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished but storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, OpenStorageDirectory_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->OpenStorageDirectory(
          PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
          OriginScope::FromNull(), Nullable<Client::Type>(),
          /* aExclusive */ false));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<UniversalDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     RefPtr<UniversalDirectoryLock> directoryLock =
                         std::move(aValue.ResolveValue());
                     DropDirectoryLock(directoryLock);

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished and an exclusive client directory lock for a non-overlapping
// origin is acquired in between.
TEST_F(TestQuotaManager,
       OpenStorageDirectory_FinishedWithExclusiveClientDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->OpenStorageDirectory(
          PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
          OriginScope::FromNull(), Nullable<Client::Type>(),
          /* aExclusive */ false));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<UniversalDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ true);

    {
      auto value = Await(directoryLock->Acquire());
      ASSERT_TRUE(value.IsResolve());
    }

    {
      auto value = Await(quotaManager->OpenStorageDirectory(
          PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
          OriginScope::FromNull(), Nullable<Client::Type>(),
          /* aExclusive */ false));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<UniversalDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirctory when an opening of a client directory is already
// ongoing and storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, OpenClientDirectory_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock;

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [&directoryLock](
                    ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  [&aValue]() { ASSERT_TRUE(aValue.ResolveValue()); }();

                  directoryLock = std::move(aValue.ResolveValue());

                  return BoolPromise::CreateAndResolve(true, __func__);
                })
            ->Then(quotaManager->IOThread(), __func__,
                   [](const BoolPromise::ResolveOrRejectValue& aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     []() {
                       QuotaManager* quotaManager = QuotaManager::Get();
                       ASSERT_TRUE(quotaManager);

                       ASSERT_TRUE(
                           quotaManager->IsStorageInitializedInternal());
                     }();

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     DropDirectoryLock(directoryLock);

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [](ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  RefPtr<ClientDirectoryLock> directoryLock =
                      std::move(aValue.ResolveValue());
                  DropDirectoryLock(directoryLock);

                  return BoolPromise::CreateAndResolve(true, __func__);
                }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory is already
// ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       OpenClientDirectory_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [&directoryLock](
                    ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  DropDirectoryLock(directoryLock);

                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  RefPtr<ClientDirectoryLock> directoryLock =
                      std::move(aValue.ResolveValue());
                  DropDirectoryLock(directoryLock);

                  return BoolPromise::CreateAndResolve(true, __func__);
                }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [](ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  RefPtr<ClientDirectoryLock> directoryLock =
                      std::move(aValue.ResolveValue());
                  DropDirectoryLock(directoryLock);

                  return BoolPromise::CreateAndResolve(true, __func__);
                }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished.
TEST_F(TestQuotaManager, OpenClientDirectory_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value =
          Await(quotaManager->OpenClientDirectory(GetTestClientMetadata()));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<ClientDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    {
      auto value =
          Await(quotaManager->OpenClientDirectory(GetTestClientMetadata()));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<ClientDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished but storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, OpenClientDirectory_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value =
          Await(quotaManager->OpenClientDirectory(GetTestClientMetadata()));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<ClientDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [](ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  RefPtr<ClientDirectoryLock> directoryLock =
                      std::move(aValue.ResolveValue());
                  DropDirectoryLock(directoryLock);

                  return BoolPromise::CreateAndResolve(true, __func__);
                }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished with an exclusive client directory lock for a different origin is
// acquired in between.
TEST_F(TestQuotaManager,
       OpenClientDirectory_FinishedWithOtherExclusiveClientDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value =
          Await(quotaManager->OpenClientDirectory(GetTestClientMetadata()));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<ClientDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetOtherTestClientMetadata(),
                                          /* aExclusive */ true);

    {
      auto value = Await(directoryLock->Acquire());
      ASSERT_TRUE(value.IsResolve());
    }

    {
      auto value =
          Await(quotaManager->OpenClientDirectory(GetTestClientMetadata()));
      ASSERT_TRUE(value.IsResolve());

      RefPtr<ClientDirectoryLock> directoryLock =
          std::move(value.ResolveValue());
      DropDirectoryLock(directoryLock);

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager, OpenClientDirectory_InitializeOrigin) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  auto backgroundTest = [](bool aInitializeOrigin) {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock;

    RefPtr<BoolPromise> promise =
        quotaManager
            ->OpenClientDirectory(GetTestClientMetadata(), aInitializeOrigin)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [&directoryLock](
                    ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  [&aValue]() { ASSERT_TRUE(aValue.ResolveValue()); }();

                  directoryLock = std::move(aValue.ResolveValue());

                  return BoolPromise::CreateAndResolve(true, __func__);
                })
            ->Then(quotaManager->IOThread(), __func__,
                   [aInitializeOrigin](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     [aInitializeOrigin]() {
                       QuotaManager* quotaManager = QuotaManager::Get();
                       ASSERT_TRUE(quotaManager);

                       ASSERT_EQ(
                           quotaManager->IsTemporaryOriginInitializedInternal(
                               GetTestOriginMetadata()),
                           aInitializeOrigin);
                     }();

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     DropDirectoryLock(directoryLock);

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   });

    {
      auto value = Await(promise);
      ASSERT_TRUE(value.IsResolve());
      ASSERT_TRUE(value.ResolveValue());
    }
  };

  ASSERT_NO_FATAL_FAILURE(
      PerformOnBackgroundThread(backgroundTest, /* aInitializeOrigin */ true));
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(
      PerformOnBackgroundThread(backgroundTest, /* aInitializeOrigin */ false));
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple InitializeStorage.
TEST_F(TestQuotaManager, InitializeStorage_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing.
TEST_F(TestQuotaManager, InitializeStorage_Ongoing) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// storage shutdown is scheduled after that. The tested InitializeStorage call
// is delayed to the point when storage shutdown is about to finish.
TEST_F(TestQuotaManager,
       InitializeStorage_OngoingWithScheduledShutdown_Delayed) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());

    OriginOperationCallbackOptions callbackOptions;
    callbackOptions.mWantWillFinishSync = true;

    OriginOperationCallbacks callbacks;
    promises.AppendElement(quotaManager->ShutdownStorage(Some(callbackOptions),
                                                         SomeRef(callbacks)));

    promises.AppendElement(callbacks.mWillFinishSyncPromise.ref()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [quotaManager = RefPtr(quotaManager)](
            const ExclusiveBoolPromise::ResolveOrRejectValue& aValue) {
          return InvokeAsync(
              GetCurrentSerialEventTarget(), __func__,
              [quotaManager]() { return quotaManager->InitializeStorage(); });
        }));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&directoryLock](const BoolPromise::ResolveOrRejectValue& aValue) {
          // The exclusive directory lock must be released when the first
          // storage initialization is finished, otherwise it would endlessly
          // block the second storage initialization.
          DropDirectoryLock(directoryLock);

          if (aValue.IsReject()) {
            return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
          }

          return BoolPromise::CreateAndResolve(true, __func__);
        }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// shared client directory locks are requested after that.
// The shared client directory locks don't have to be released in this case.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithClientDirectoryLocks) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
    DropDirectoryLock(directoryLock2);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// shared client directory locks are requested after that with storage shutdown
// scheduled in between.
TEST_F(TestQuotaManager,
       InitializeStorage_OngoingWithClientDirectoryLocksAndScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    directoryLock->OnInvalidate(
        [&directoryLock]() { DropDirectoryLock(directoryLock); });

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock2);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished.
TEST_F(TestQuotaManager, InitializeStorage_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished but
// storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, InitializeStorage_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished and
// shared client directory locks are requested immediately after requesting
// storage initialization.
TEST_F(TestQuotaManager, InitializeStorage_FinishedWithClientDirectoryLocks) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
    DropDirectoryLock(directoryLock2);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished and
// shared client directory locks are requested immediatelly after requesting
// storage initialization with storage shutdown performed in between.
// The shared client directory lock is released when it gets invalidated by
// storage shutdown which then unblocks the shutdown.
TEST_F(TestQuotaManager,
       InitializeStorage_FinishedWithClientDirectoryLocksAndScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    directoryLock->OnInvalidate(
        [&directoryLock]() { DropDirectoryLock(directoryLock); });

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    {
      auto value = Await(quotaManager->ShutdownStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_FALSE(quotaManager->IsStorageInitialized());
    }

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    DropDirectoryLock(directoryLock2);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializePersistentStorage_OtherExclusiveDirectoryLockAcquired) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_TEMPORARY,
                                            PERSISTENCE_TYPE_DEFAULT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ true);

    {
      auto value = Await(directoryLock->Acquire());
      ASSERT_TRUE(value.IsResolve());
    }

    {
      auto value = Await(quotaManager->InitializePersistentStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializePersistentStorage when a persistent storage initialization is
// already ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       InitializePersistentStorage_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&directoryLock](const BoolPromise::ResolveOrRejectValue& aValue) {
          // The exclusive directory lock must be released when the first
          // Persistent storage initialization is finished, otherwise it would
          // endlessly block the second persistent storage initialization.
          DropDirectoryLock(directoryLock);

          if (aValue.IsReject()) {
            return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
          }

          return BoolPromise::CreateAndResolve(true, __func__);
        }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializePersistentStorage when a persistent storage initialization
// already finished.
TEST_F(TestQuotaManager, InitializePersistentStorage_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializePersistentStorage_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializePersistentStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsPersistentStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryStorage_OtherExclusiveDirectoryLockAcquired) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ true);

    {
      auto value = Await(directoryLock->Acquire());
      ASSERT_TRUE(value.IsResolve());
    }

    {
      auto value = Await(quotaManager->InitializeTemporaryStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }

    DropDirectoryLock(directoryLock);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeTemporaryStorage when a temporary storage initialization is
// already ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       InitializeTemporaryStorage_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromNull(), OriginScope::FromNull(),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&directoryLock](const BoolPromise::ResolveOrRejectValue& aValue) {
          // The exclusive directory lock must be dropped when the first
          // temporary storage initialization is finished, otherwise it would
          // endlessly block the second temporary storage initialization.
          DropDirectoryLock(directoryLock);

          if (aValue.IsReject()) {
            return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
          }

          return BoolPromise::CreateAndResolve(true, __func__);
        }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeTemporaryStorage when a temporary storage initialization
// already finished.
TEST_F(TestQuotaManager, InitializeTemporaryStorage_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryStorage_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryGroup_OtherExclusiveDirectoryLockAcquired) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->InitializeStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
    }

    {
      auto value = Await(quotaManager->InitializeTemporaryStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
    }

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromValue(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromGroup(testOriginMetadata.mGroup),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    {
      auto value = Await(directoryLock->Acquire());
      ASSERT_TRUE(value.IsResolve());
    }

    {
      auto value =
          Await(quotaManager->InitializeTemporaryGroup(testOriginMetadata));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }

    DropDirectoryLock(directoryLock);
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeTemporaryGroup when a temporary group initialization is
// already ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       InitializeTemporaryGroup_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(
            PersistenceScope::CreateFromSet(PERSISTENCE_TYPE_TEMPORARY,
                                            PERSISTENCE_TYPE_DEFAULT),
            OriginScope::FromGroup(testOriginMetadata.mGroup),
            Nullable<Client::Type>(),
            /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     // The exclusive directory lock must be dropped when the
                     // first temporary group initialization is finished,
                     // otherwise it would endlessly block the second temporary
                     // group initialization.
                     DropDirectoryLock(directoryLock);

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeTemporaryGroup when a temporary group initialization already
// finished.
TEST_F(TestQuotaManager, InitializeTemporaryGroup_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryGroup_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(
        quotaManager->InitializeTemporaryGroup(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryGroupInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializePersistentOrigin_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestPersistentOriginMetadata();

    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(
        quotaManager->InitializePersistentOrigin(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsPersistentOriginInitialized(testOriginMetadata));
    }

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(
        quotaManager->InitializePersistentOrigin(testOriginMetadata));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsPersistentOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryOrigin_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryOrigin(
        testOriginMetadata,
        /* aCreateIfNonExistent */ false));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryOrigin(
        testOriginMetadata,
        /* aCreateIfNonExistent */ true));

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_TRUE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ClearStoragesForOrigin.
TEST_F(TestQuotaManager, ClearStoragesForOrigin_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(
      InitializeTemporaryOrigin(GetTestOriginMetadata(),
                                /* aCreateIfNonExistent */ true));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOrigin(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager, ClearStoragesForOrigin_NonExistentOriginDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ false));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOrigin(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ClearStoragesForOriginPrefix.
TEST_F(TestQuotaManager, ClearStoragesForOriginPrefix_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ true));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOriginPrefix(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       ClearStoragesForOriginPrefix_NonExistentOriginDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ false));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOriginPrefix(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ClearStoragesForOriginAttributesPattern.
TEST_F(TestQuotaManager, ClearStoragesForOriginAttributesPattern_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ true));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOriginAttributesPattern(
          OriginAttributesPattern()));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       ClearStoragesForOriginAttributesPattern_NonExistentOriginDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ false));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ClearStoragesForOriginAttributesPattern(
          OriginAttributesPattern()));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ShutdownStoragesForOrigin.
TEST_F(TestQuotaManager, ShutdownStoragesForOrigin_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ true));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ShutdownStoragesForOrigin(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager, ShutdownStoragesForOrigin_NonExistentOriginDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginNotInitialized(GetTestOriginMetadata()));

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin(
      GetTestOriginMetadata(), /* aCreateIfNonExistent */ false));

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageInitialized());
  ASSERT_NO_FATAL_FAILURE(
      AssertTemporaryOriginInitialized(GetTestOriginMetadata()));

  PerformOnBackgroundThread([]() {
    auto testOriginMetadata = GetTestOriginMetadata();

    nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateContentPrincipal(testOriginMetadata.mOrigin);
    QM_TRY(MOZ_TO_RESULT(principal), QM_TEST_FAIL);

    mozilla::ipc::PrincipalInfo principalInfo;
    QM_TRY(MOZ_TO_RESULT(PrincipalToPrincipalInfo(principal, &principalInfo)),
           QM_TEST_FAIL);

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ShutdownStoragesForOrigin(
          /* aPersistenceType */ Nothing(), principalInfo));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_TRUE(quotaManager->IsStorageInitialized());
      ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());
      ASSERT_FALSE(
          quotaManager->IsTemporaryOriginInitialized(testOriginMetadata));
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ShutdownStorage.
TEST_F(TestQuotaManager, ShutdownStorage_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    {
      auto value = Await(quotaManager->ShutdownStorage());
      ASSERT_TRUE(value.IsResolve());

      ASSERT_FALSE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing.
TEST_F(TestQuotaManager, ShutdownStorage_Ongoing) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_FALSE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing and storage
// initialization is scheduled after that.
TEST_F(TestQuotaManager, ShutdownStorage_OngoingWithScheduledInitialization) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());

    {
      auto value =
          Await(BoolPromise::All(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());

      ASSERT_FALSE(quotaManager->IsStorageInitialized());
    }
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing and a shared
// client directory lock is requested after that.
// The shared client directory lock doesn't have to be explicitly released
// because it gets invalidated while it's still pending which causes that any
// directory locks that were blocked by the shared client directory lock become
// unblocked.
TEST_F(TestQuotaManager, ShutdownStorage_OngoingWithClientDirectoryLock) {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    // This creates an exclusive directory lock internally.
    promises.AppendElement(quotaManager->ShutdownStorage());

    // This directory lock can't be acquired yet because a storage shutdown
    // (which uses an exclusive diretory lock internall) is ongoing.
    promises.AppendElement(directoryLock->Acquire());

    // This second ShutdownStorage invalidates the directoryLock, so that
    // directory lock can't ever be successfully acquired, the promise for it
    // will be rejected when the first ShutdownStorage is finished (it
    // releases its exclusive directory lock);
    promises.AppendElement(quotaManager->ShutdownStorage());

    {
      auto value = Await(
          BoolPromise::AllSettled(GetCurrentSerialEventTarget(), promises));
      ASSERT_TRUE(value.IsResolve());
    }
  });
}

TEST_F(TestQuotaManagerAndShutdownFixture,
       ThumbnailPrivateIdentityTemporaryOriginCount) {
  PerformOnIOThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    const uint32_t thumbnailPrivateIdentityId =
        quotaManager->GetThumbnailPrivateIdentityId();

    {
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(GetFullOriginMetadata(
          ""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                                "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.com"_ns,
                                "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                1u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.com"_ns, "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigin(GetFullOriginMetadata(
          ""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                                "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.com"_ns,
                                "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                1u);

      quotaManager->RemoveTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.com"_ns, "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);
    }

    {
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(GetFullOriginMetadata(
          ""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                                "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.com"_ns,
                                "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                1u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.com"_ns, "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigins(PERSISTENCE_TYPE_TEMPORARY);
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigins(PERSISTENCE_TYPE_DEFAULT);
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);
    }

    {
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(GetFullOriginMetadata(
          ""_ns, "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.org"_ns,
                                "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata("^userContextId=1"_ns, "mozilla.com"_ns,
                                "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.org"_ns, "http://www.mozilla.org"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                1u);

      quotaManager->AddTemporaryOrigin(
          GetFullOriginMetadata(nsFmtCString(FMT_STRING("^userContextId={}"),
                                             thumbnailPrivateIdentityId),
                                "mozilla.com"_ns, "http://www.mozilla.com"_ns));
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                2u);

      quotaManager->RemoveTemporaryOrigins();
      ASSERT_EQ(quotaManager->ThumbnailPrivateIdentityTemporaryOriginCount(),
                0u);
    }
  });
}

}  // namespace mozilla::dom::quota::test
