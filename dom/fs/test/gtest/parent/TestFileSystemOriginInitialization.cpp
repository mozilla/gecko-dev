/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemParentTest.h"
#include "FileSystemParentTestHelpers.h"
#include "FileSystemParentTypes.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/dom/FileSystemQuotaClient.h"
#include "mozilla/dom/FileSystemQuotaClientFactory.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

// This file is intended for integration tests which verify origin
// initialization.

namespace mozilla::dom::fs::test {

using ::testing::_;
using ::testing::Expectation;
using ::testing::Sequence;

namespace {

class MockFileSystemQuotaClient : public FileSystemQuotaClient {
 public:
  MOCK_METHOD((Result<quota::UsageInfo, nsresult>), InitOrigin,
              (quota::PersistenceType aPersistenceType,
               const quota::OriginMetadata& aOriginMetadata,
               const AtomicBool& aCanceled),
              (override));

  MOCK_METHOD((Result<quota::UsageInfo, nsresult>), GetUsageForOrigin,
              (quota::PersistenceType aPersistenceType,
               const quota::OriginMetadata& aOriginMetadata,
               const AtomicBool& aCanceled),
              (override));

  void DelegateToBase() {
    // This exists just to workaround the false positive:
    // ERROR: Refcounted variable 'this' of type 'mozilla::dom::fs::test::
    // (anonymous namespace)::MockFileSystemQuotaClient' cannot be captured by
    // a lambda
    bool dummy;

    ON_CALL(*this, InitOrigin)
        .WillByDefault(
            [&dummy, this](quota::PersistenceType aPersistenceType,
                           const quota::OriginMetadata& aOriginMetadata,
                           const Atomic<bool>& aCanceled) {
              (void)dummy;
              return FileSystemQuotaClient::InitOrigin(
                  aPersistenceType, aOriginMetadata, aCanceled);
            });

    ON_CALL(*this, GetUsageForOrigin)
        .WillByDefault(
            [&dummy, this](quota::PersistenceType aPersistenceType,
                           const quota::OriginMetadata& aOriginMetadata,
                           const Atomic<bool>& aCanceled) {
              (void)dummy;
              return FileSystemQuotaClient::GetUsageForOrigin(
                  aPersistenceType, aOriginMetadata, aCanceled);
            });
  }
};

class TestFileSystemQuotaClientFactory final
    : public FileSystemQuotaClientFactory {
 public:
  already_AddRefed<MockFileSystemQuotaClient> GetQuotaClient() {
    return do_AddRef(mQuotaClient);
  }

 protected:
  already_AddRefed<quota::Client> AllocQuotaClient() override {
    mQuotaClient = MakeRefPtr<MockFileSystemQuotaClient>();
    mQuotaClient->DelegateToBase();
    return do_AddRef(mQuotaClient);
  }

  RefPtr<MockFileSystemQuotaClient> mQuotaClient;
};

}  // namespace

class TestFileSystemOriginInitialization : public FileSystemParentTest {
 protected:
  static void SetUpTestCase() {
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    prefs->SetBoolPref("dom.quotaManager.loadQuotaFromCache", false);

    auto factory = MakeRefPtr<TestFileSystemQuotaClientFactory>();

    FileSystemQuotaClientFactory::SetCustomFactory(factory);

    ASSERT_NO_FATAL_FAILURE(FileSystemParentTest::SetUpTestCase());

    sQuotaClient = factory->GetQuotaClient();
  }

  static void TearDownTestCase() {
    sQuotaClient = nullptr;

    ASSERT_NO_FATAL_FAILURE(FileSystemParentTest::TearDownTestCase());

    FileSystemQuotaClientFactory::SetCustomFactory(nullptr);
  }

  MOZ_RUNINIT static inline RefPtr<MockFileSystemQuotaClient> sQuotaClient;
};

TEST_F(TestFileSystemOriginInitialization, EmptyOriginDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).Times(0).InSequence(s);

    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _))
        .Times(0)
        .InSequence(s);
  }

  // Initialize origin
  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(
      InitializeTemporaryOrigin(/* aCreateIfNonExistent */ true));

  // After initialization,
  // * origin usage is nothing
  // * cached origin usage is zero
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Shutdown temporary storage
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still nothing
  // * cached origin is still zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(AssertTemporaryStorageNotInitialized());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still nothing
  // * cached origin is still zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));
}

TEST_F(TestFileSystemOriginInitialization, EmptyFileSystemDirectory) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).Times(0).InSequence(s);

    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _))
        .Times(0)
        .InSequence(s);
  }

  // Initialize client
  ASSERT_NO_FATAL_FAILURE(InitializeStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryOrigin());
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryClient());

  // After initialization,
  // * origin usage is nothing
  // * cached origin usage is zero
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still nothing
  // * cached origin usage is still zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still nothing
  // * cached origin usage is still zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));
}

TEST_F(TestFileSystemOriginInitialization, EmptyFileSystemDatabase) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    // GetOriginUsage check after database initialization.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check when temporary storage is not initialized.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Repeated temporary storage initialization.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Final GetOriginUsage check.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);
  }

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // After initialization,
  // * origin usage is not zero
  // * GetOriginUsage and GetCachedOriginUsage should agree
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t beforeShutdownUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ReleaseDataManager());
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still the same as before shutdown
  // * cached origin usage is zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still the same as before shutdown
  // * GetOriginUsage and GetCachedOriginUsage should agree again
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));
}

TEST_F(TestFileSystemOriginInitialization, EmptyFileSystemFile) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    // GetOriginUsage check after file creation.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check when temporary storage is not initialized.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Repeated temporary storage initialization.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Final GetOriginUsage check.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);
  }

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // After a new file has been created (only in the database),
  // * origin usage is not zero
  // * GetOriginUsage and GetCachedOriginUsage should agree
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t beforeShutdownUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ReleaseDataManager());
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still the same as before shutdown
  // * cached origin usage is zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still the same as before shutdown
  // * GetOriginUsage and GetCachedOriginUsage should agree
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));
}

TEST_F(TestFileSystemOriginInitialization, NonEmptyFileSystemFile) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    // GetOriginUsage check after filling the file with content.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check after unlocking the file..
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check when temporary storage is not initialized.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Repeated temporary storage initialization.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Final GetOriginUsage check.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);
  }

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // Fill the file with some content
  ASSERT_NO_FATAL_FAILURE(LockExclusive(testFileId));

  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  // After the content has been written to the file,
  // * origin usage is not zero
  // * GetOriginUsage and GetCachedOriginUsage should agree
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t beforeShutdownUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(UnlockExclusive(testFileId));

  // After the file has been unlocked,
  // * origin usage is still the same as before unlocking
  // * GetOriginUsage and GetCachedOriginUsage should still agree
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ReleaseDataManager());
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still the same as before shutdown
  // * cached origin usage is zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still the same as before shutdown
  // * GetOriginUsage and GetCachedOriginUsage should agree again
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));
}

TEST_F(TestFileSystemOriginInitialization,
       NonEmptyFileSystemFile_UncleanShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    // GetOriginUsage check after file creation.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check after filling the file with content.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check when temporary storage is not initialized.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Repeated temporary storage initialization.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Final GetOriginUsage check.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);
  }

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // After a new file has been created (only in the database),
  // * origin usage is not zero
  // * GetOriginUsage and GetCachedOriginUsage should agree
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t beforeWriteUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeWriteUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeWriteUsage));

  // Fill the file with some content
  ASSERT_NO_FATAL_FAILURE(LockExclusive(testFileId));

  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  // After the content has been written to the file,
  // * origin usage is not the same as before writing
  // * GetOriginUsage and GetCachedOriginUsage should still agree
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, beforeWriteUsage));

  uint64_t beforeShutdownUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  // UnlockExclusive is not called here on purpose to simmulate unclean
  // shutdown.

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ReleaseDataManager());
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * static database usage is the same as before writing
  // * origin usage is still the same as before shutdown
  // * cached origin usage is zero
  ASSERT_NO_FATAL_FAILURE(GetStaticDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeWriteUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * static database usage is the same as before shutdown
  // * GetStaticDatabaseUsage, GetOriginUsage and GetCachedOriginUsage should
  // all agree again
  ASSERT_NO_FATAL_FAILURE(GetStaticDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));
}

TEST_F(TestFileSystemOriginInitialization, RemovedFileSystemFile) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  // Set expectations
  {
    Sequence s;

    // GetOriginUsage check after removing the file..
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);

    // GetOriginUsage check when temporary storage is not initialized.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Repeated temporary storage initialization.
    EXPECT_CALL(*sQuotaClient, InitOrigin(_, _, _)).InSequence(s);

    // Final GetOriginUsage check.
    EXPECT_CALL(*sQuotaClient, GetUsageForOrigin(_, _, _)).InSequence(s);
  }

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // Fill the file with some content
  ASSERT_NO_FATAL_FAILURE(LockExclusive(testFileId));

  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  ASSERT_NO_FATAL_FAILURE(UnlockExclusive(testFileId));

  // Remove the file
  bool wasRemoved;
  ASSERT_NO_FATAL_FAILURE(RemoveFile(wasRemoved));
  ASSERT_TRUE(wasRemoved);

  // After the file has been removed,
  // * origin usage is not zero
  // * GetOriginUsage and GetCachedOriginUsage should agree
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t beforeShutdownUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  // Shutdown temporary storage.
  ASSERT_NO_FATAL_FAILURE(ReleaseDataManager());
  ASSERT_NO_FATAL_FAILURE(ShutdownTemporaryStorage());

  // After temporary storage shutdown,
  // * origin usage is still the same as before shutdown
  // * cached origin usage is zero
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, 0u));

  // Initialize temporary storage again.
  ASSERT_NO_FATAL_FAILURE(InitializeTemporaryStorage());

  // After repeated temporary storage initialization,
  // * origin usage is still the same as before shutdown
  // * GetOriginUsage and GetCachedOriginUsage should agree again
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));

  ASSERT_NO_FATAL_FAILURE(GetCachedOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, beforeShutdownUsage));
}

}  // namespace mozilla::dom::fs::test
