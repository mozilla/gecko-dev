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
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/dom/quota/test/QuotaManagerDependencyFixture.h"

// This file is intended for integration tests which verify usage tracking
// without any restart in between.

namespace mozilla::dom::fs::test {

class TestFileSystemUsageTracking
    : public quota::test::QuotaManagerDependencyFixture {
 protected:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }

  void TearDown() override {
    PerformOnBackgroundThread([this]() { mDataManager = nullptr; });

    ASSERT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));
  }

  static void GetOriginUsage(quota::UsageInfo& aResult) {
    ASSERT_NO_FATAL_FAILURE(QuotaManagerDependencyFixture::GetOriginUsage(
        GetTestOriginMetadata(), &aResult));
  }

  void EnsureDataManager() {
    PerformOnBackgroundThread([this]() {
      ASSERT_NO_FATAL_FAILURE(test::CreateRegisteredDataManager(
          GetTestOriginMetadata(), mDataManager));
    });
  }

  void CreateNewEmptyFile(EntryId& aEntryId) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY_UNWRAP(
        EntryId testFileId,
        PerformOnThread(mDataManager->MutableIOTaskQueuePtr(),
                        [this]() -> Result<EntryId, QMResult> {
                          data::FileSystemDatabaseManager* databaseManager =
                              mDataManager->MutableDatabaseManagerPtr();

                          QM_TRY_UNWRAP(const EntryId rootId,
                                        data::GetRootHandle(GetTestOrigin()));
                          FileSystemChildMetadata fileData(rootId,
                                                           GetTestFileName());

                          EntryId testFileId;
                          ENSURE_NO_FATAL_FAILURE(
                              test::CreateNewEmptyFile(databaseManager,
                                                       fileData, testFileId),
                              Err(QMResult(NS_ERROR_FAILURE)));

                          return testFileId;
                        }));

    aEntryId = testFileId;
  }

  void WriteDataToFile(EntryId& aEntryId, const nsCString& aData) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY(PerformOnThread(
        mDataManager->MutableIOTaskQueuePtr(),
        [this, &aEntryId, &aData]() -> Result<Ok, QMResult> {
          data::FileSystemDatabaseManager* databaseManager =
              mDataManager->MutableDatabaseManagerPtr();

          ENSURE_NO_FATAL_FAILURE(
              test::WriteDataToFile(GetTestOriginMetadata(), databaseManager,
                                    aEntryId, aData),
              Err(QMResult(NS_ERROR_FAILURE)));

          return Ok{};
        }));
  }

  void RemoveFile(bool& aWasRemoved) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY_UNWRAP(
        bool wasRemoved,
        PerformOnThread(mDataManager->MutableIOTaskQueuePtr(),
                        [this]() -> Result<bool, QMResult> {
                          data::FileSystemDatabaseManager* databaseManager =
                              mDataManager->MutableDatabaseManagerPtr();

                          QM_TRY_UNWRAP(const EntryId rootId,
                                        data::GetRootHandle(GetTestOrigin()));

                          QM_TRY_RETURN(databaseManager->RemoveFile(
                              {rootId, GetTestFileName()}));
                        }));

    aWasRemoved = wasRemoved;
  }

  void GetDatabaseUsage(quota::UsageInfo& aDatabaseUsage) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY_UNWRAP(
        auto databaseUsage,
        PerformOnThread(mDataManager->MutableIOTaskQueuePtr(), [this]() {
          data::FileSystemDatabaseManager* databaseManager =
              mDataManager->MutableDatabaseManagerPtr();

          QM_TRY_RETURN(databaseManager->GetUsage());
        }));

    aDatabaseUsage = databaseUsage;
  }

  void UpdateDatabaseUsage(const FileId& aFileId) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY(PerformOnThread(
        mDataManager->MutableIOTaskQueuePtr(), [this, &aFileId]() {
          data::FileSystemDatabaseManager* databaseManager =
              mDataManager->MutableDatabaseManagerPtr();

          QM_TRY_RETURN(MOZ_TO_RESULT(databaseManager->UpdateUsage(aFileId)));
        }));
  }

 private:
  Registered<data::FileSystemDataManager> mDataManager;
};

TEST_F(TestFileSystemUsageTracking, CheckUsageBeforeAnyFilesOnDisk) {
  // For uninitialized database, file usage is nothing
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_TRUE(usageNow.DatabaseUsage().isNothing());
  EXPECT_TRUE(usageNow.FileUsage().isNothing());

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // After initialization,
  // * database size is not zero
  // * GetDatabaseUsage and GetOriginUsage should agree
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));
  const auto initialDbUsage = usageNow.DatabaseUsage().value();

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  // Create a new file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // After a new file has been created (only in the database),
  // * database size has increased
  // * GetOriginUsage and GetDatabaseUsage should agree
  const auto expectedUse = initialDbUsage + 2 * GetPageSize();

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));

  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));
}

TEST_F(TestFileSystemUsageTracking, WritesToFilesShouldIncreaseUsage) {
  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
  const auto testFileDbUsage = usageNow.DatabaseUsage().value();

  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

  // Fill the file with some content
  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  // In this test we don't lock the file -> no rescan is expected
  // and GetDatabaseUsage should return the previous value
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

  // When data manager unlocks the file, it should call update
  // but in this test we call it directly
  ASSERT_NO_FATAL_FAILURE(UpdateDatabaseUsage(FileId(testFileId)));

  const auto expectedTotalUsage = testFileDbUsage + testData.Length();

  // Disk usage should have increased after writing
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

  // The usage values should now agree
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));
}

TEST_F(TestFileSystemUsageTracking, RemovingFileShouldDecreaseUsage) {
  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
  const auto testFileDbUsage = usageNow.DatabaseUsage().value();

  // Fill the file with some content
  const nsCString& testData = GetTestData();
  const auto expectedTotalUsage = testFileDbUsage + testData.Length();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  // Currently, usage is expected to be updated on unlock by data manager
  // but here UpdateUsage() is called directly
  ASSERT_NO_FATAL_FAILURE(UpdateDatabaseUsage(FileId(testFileId)));

  // At least some file disk usage should have appeared after unlocking
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

  bool wasRemoved;
  ASSERT_NO_FATAL_FAILURE(RemoveFile(wasRemoved));
  ASSERT_TRUE(wasRemoved);

  // Removes cascade and usage table should be up to date immediately
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

  // GetOriginUsage should agree
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));
}

}  // namespace mozilla::dom::fs::test
