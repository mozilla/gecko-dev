/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemParentTestHelpers.h"
#include "FileSystemParentTypes.h"
#include "TestHelpers.h"
#include "datamodel/FileSystemDataManager.h"
#include "datamodel/FileSystemDatabaseManager.h"
#include "gtest/gtest.h"
#include "mozilla/dom/FileSystemQuotaClientFactory.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/dom/quota/test/QuotaManagerDependencyFixture.h"

// This file is intended for integration tests which verify usage tracking
// without any restart in between.

namespace mozilla::dom::fs::test {

class TestFileSystemUsageTracking
    : public quota::test::QuotaManagerDependencyFixture {
 protected:
  void SetUp() override { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  void TearDown() override {
    EXPECT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));
    ASSERT_NO_FATAL_FAILURE(ShutdownFixture());
  }
};

TEST_F(TestFileSystemUsageTracking, CheckUsageBeforeAnyFilesOnDisk) {
  auto backgroundTask = []() {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](const RefPtr<quota::Client>& quotaClient,
                                data::FileSystemDatabaseManager* dbm) {
      ASSERT_FALSE(isCanceled);
      const quota::OriginMetadata& testOriginMeta = GetTestOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      // After initialization,
      // * database size is not zero
      // * GetUsageForOrigin and InitOrigin should agree
      TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));
      const auto initialDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

      // Create a new file
      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));

      // After a new file has been created (only in the database),
      // * database size has increased
      // * GetUsageForOrigin and InitOrigin should agree
      const auto expectedUse = initialDbUsage + 2 * GetPageSize();

      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));

      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    ASSERT_TRUE(quotaClient);

    // For uninitialized database, file usage is nothing
    auto checkTask =
        [&isCanceled](const RefPtr<mozilla::dom::quota::Client>& quotaClient) {
          TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                          quotaClient->GetUsageForOrigin(
                              quota::PERSISTENCE_TYPE_DEFAULT,
                              GetTestOriginMetadata(), isCanceled));

          ASSERT_TRUE(usageNow.DatabaseUsage().isNothing());
          EXPECT_TRUE(usageNow.FileUsage().isNothing());
        };

    PerformOnIOThread(std::move(checkTask),
                      RefPtr<mozilla::dom::quota::Client>{quotaClient});

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    ASSERT_NO_FATAL_FAILURE(
        CreateRegisteredDataManager(GetTestOriginMetadata(), rdm));

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

TEST_F(TestFileSystemUsageTracking, WritesToFilesShouldIncreaseUsage) {
  auto backgroundTask = []() {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](
                      const RefPtr<mozilla::dom::quota::Client>& quotaClient,
                      data::FileSystemDatabaseManager* dbm) {
      const quota::OriginMetadata& testOriginMeta = GetTestOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));
      // const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(
          quota::UsageInfo usageNow,
          quotaClient->GetUsageForOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                         testOriginMeta, isCanceled));
      ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
      const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // Fill the file with some content
      const nsCString& testData = GetTestData();

      ASSERT_NO_FATAL_FAILURE(
          WriteDataToFile(GetTestOriginMetadata(), dbm, testFileId, testData));

      // In this test we don't lock the file -> no rescan is expected
      // and InitOrigin should return the previous value
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // When data manager unlocks the file, it should call update
      // but in this test we call it directly
      ASSERT_NSEQ(NS_OK, dbm->UpdateUsage(FileId(testFileId)));

      const auto expectedTotalUsage = testFileDbUsage + testData.Length();

      // Disk usage should have increased after writing
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

      // The usage values should now agree
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    ASSERT_TRUE(quotaClient);

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    ASSERT_NO_FATAL_FAILURE(
        CreateRegisteredDataManager(GetTestOriginMetadata(), rdm));

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

TEST_F(TestFileSystemUsageTracking, RemovingFileShouldDecreaseUsage) {
  auto backgroundTask = []() {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](
                      const RefPtr<mozilla::dom::quota::Client>& quotaClient,
                      data::FileSystemDatabaseManager* dbm) {
      const quota::OriginMetadata& testOriginMeta = GetTestOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));
      TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
      const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      // Fill the file with some content
      const nsCString& testData = GetTestData();
      const auto expectedTotalUsage = testFileDbUsage + testData.Length();

      ASSERT_NO_FATAL_FAILURE(
          WriteDataToFile(GetTestOriginMetadata(), dbm, testFileId, testData));

      // Currently, usage is expected to be updated on unlock by data manager
      // but here UpdateUsage() is called directly
      ASSERT_NSEQ(NS_OK, dbm->UpdateUsage(FileId(testFileId)));

      // At least some file disk usage should have appeared after unlocking
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

      TEST_TRY_UNWRAP(bool wasRemoved,
                      dbm->RemoveFile({rootId, GetTestFileName()}));
      ASSERT_TRUE(wasRemoved);

      // Removes cascade and usage table should be up to date immediately
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // GetUsageForOrigin should agree
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));

      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    ASSERT_TRUE(quotaClient);

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    ASSERT_NO_FATAL_FAILURE(
        CreateRegisteredDataManager(GetTestOriginMetadata(), rdm));

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());
  };

  PerformOnBackgroundThread(std::move(backgroundTask));
}

}  // namespace mozilla::dom::fs::test
