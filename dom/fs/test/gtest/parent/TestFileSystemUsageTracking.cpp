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

  void LockExclusive(const EntryId& aEntryId) {
    ASSERT_TRUE(mDataManager);

    TEST_TRY_UNWRAP(FileId fileId,
                    PerformOnBackgroundThread([this, &aEntryId]() {
                      return mDataManager->LockExclusive(aEntryId);
                    }));
  }

  void UnlockExclusive(const EntryId& aEntryId) {
    ASSERT_TRUE(mDataManager);

    PerformOnBackgroundThread(
        [this, &aEntryId]() { mDataManager->UnlockExclusive(aEntryId); });
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
  // For uninitialized database, origin usage is nothing
  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageIsNothing(usageNow));

  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // After initialization,
  // * database usage is not zero
  // * GetDatabaseUsage and GetOriginUsage should agree
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t initialDbUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, initialDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  // After a new file has been created (only in the database),
  // * database usage has increased
  // * GetDatabaseUsage and GetOriginUsage should agree
  const auto increasedDbUsage = initialDbUsage + 2 * GetPageSize();

  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));
}

TEST_F(TestFileSystemUsageTracking, WritesToFilesShouldIncreaseUsage) {
  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t initialDbUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, initialDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  // Fill the file with some content
  ASSERT_NO_FATAL_FAILURE(LockExclusive(testFileId));

  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  // After the content has been written to the file,
  // * database usage is the same (the usage is updated later during file
  //   unlocking)
  // * origin usage has increased
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  const auto increasedDbUsage = initialDbUsage + testData.Length();

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));

  ASSERT_NO_FATAL_FAILURE(UnlockExclusive(testFileId));

  // After the file has been unlocked,
  // * database usage has increased
  // * GetDatabaseUsage and GetOriginUsage should now agree
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));
}

TEST_F(TestFileSystemUsageTracking, RemovingFileShouldDecreaseUsage) {
  // Initialize database
  ASSERT_NO_FATAL_FAILURE(EnsureDataManager());

  // Create a new empty file
  EntryId testFileId;
  ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(testFileId));

  quota::UsageInfo usageNow;
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));

  uint64_t initialDbUsage;
  ASSERT_NO_FATAL_FAILURE(GetUsageValue(usageNow, initialDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  // Fill the file with some content
  ASSERT_NO_FATAL_FAILURE(LockExclusive(testFileId));

  const nsCString& testData = GetTestData();

  ASSERT_NO_FATAL_FAILURE(WriteDataToFile(testFileId, testData));

  ASSERT_NO_FATAL_FAILURE(UnlockExclusive(testFileId));

  // After the file has been unlocked,
  // * database usage has increased
  // * GetDatabaseUsage and GetOriginUsage should now agree
  const auto increasedDbUsage = initialDbUsage + testData.Length();

  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, increasedDbUsage));

  // Remove the file
  bool wasRemoved;
  ASSERT_NO_FATAL_FAILURE(RemoveFile(wasRemoved));
  ASSERT_TRUE(wasRemoved);

  // After the file has been removed,
  // * database usage has decreased (to the initial value)
  // * GetDatabaseUsage and GetOriginUsage should agree
  ASSERT_NO_FATAL_FAILURE(GetDatabaseUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

  ASSERT_NO_FATAL_FAILURE(GetOriginUsage(usageNow));
  ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));
}

}  // namespace mozilla::dom::fs::test
