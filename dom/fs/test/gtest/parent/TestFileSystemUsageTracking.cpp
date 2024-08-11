/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemParentTest.h"
#include "FileSystemParentTestHelpers.h"
#include "FileSystemParentTypes.h"
#include "gtest/gtest.h"
#include "mozilla/dom/quota/UsageInfo.h"

// This file is intended for integration tests which verify usage tracking
// without any restart in between.

namespace mozilla::dom::fs::test {

class TestFileSystemUsageTracking : public FileSystemParentTest {};

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
