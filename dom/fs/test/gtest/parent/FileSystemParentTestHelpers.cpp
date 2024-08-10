/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemParentTestHelpers.h"

#include <algorithm>

#include "FileSystemParentTypes.h"
#include "TestHelpers.h"
#include "datamodel/FileSystemDataManager.h"
#include "datamodel/FileSystemDatabaseManager.h"
#include "gtest/gtest.h"
#include "mozilla/Result.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "nsIFile.h"
#include "nsString.h"

namespace mozilla::dom::fs::test {

namespace {

const int sPage = 64 * 512;

// ExceedsPreallocation value may depend on platform and sqlite version!
const int sExceedsPreallocation = sPage;

}  // namespace

int GetPageSize() { return sPage; }

const Name& GetTestFileName() {
  static Name testFileName = []() {
    nsCString testCFileName;
    testCFileName.SetLength(sExceedsPreallocation);
    std::fill(testCFileName.BeginWriting(), testCFileName.EndWriting(), 'x');
    return NS_ConvertASCIItoUTF16(testCFileName.BeginReading(),
                                  sExceedsPreallocation);
  }();

  return testFileName;
}

uint64_t BytesOfName(const Name& aName) {
  return static_cast<uint64_t>(aName.Length() * sizeof(Name::char_type));
}

const nsCString& GetTestData() {
  static const nsCString sTestData = "There is a way out of every box"_ns;
  return sTestData;
}

void CreateNewEmptyFile(data::FileSystemDatabaseManager* const aDatabaseManager,
                        const FileSystemChildMetadata& aFileSlot,
                        EntryId& aEntryId) {
  // The file should not exist yet
  Result<EntryId, QMResult> existingTestFile =
      aDatabaseManager->GetOrCreateFile(aFileSlot, /* create */ false);
  ASSERT_TRUE(existingTestFile.isErr());
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR,
              ToNSResult(existingTestFile.unwrapErr()));

  // Create a new file
  TEST_TRY_UNWRAP(aEntryId, aDatabaseManager->GetOrCreateFile(
                                aFileSlot, /* create */ true));
}

void WriteDataToFile(const quota::OriginMetadata& aOriginMetadata,
                     data::FileSystemDatabaseManager* const aDatabaseManager,
                     const EntryId& aEntryId, const nsCString& aData) {
  TEST_TRY_UNWRAP(FileId fileId, aDatabaseManager->EnsureFileId(aEntryId));
  ASSERT_FALSE(fileId.IsEmpty());

  ContentType type;
  TimeStamp lastModMilliS = 0;
  Path path;
  nsCOMPtr<nsIFile> fileObj;
  ASSERT_NSEQ(NS_OK,
              aDatabaseManager->GetFile(aEntryId, fileId, FileMode::EXCLUSIVE,
                                        type, lastModMilliS, path, fileObj));

  uint32_t written = 0;
  ASSERT_NE(written, aData.Length());

  TEST_TRY_UNWRAP(nsCOMPtr<nsIOutputStream> fileStream,
                  quota::CreateFileOutputStream(
                      quota::PERSISTENCE_TYPE_DEFAULT, aOriginMetadata,
                      quota::Client::FILESYSTEM, fileObj));

  auto finallyClose = MakeScopeExit(
      [&fileStream]() { ASSERT_NSEQ(NS_OK, fileStream->Close()); });
  ASSERT_NSEQ(NS_OK, fileStream->Write(aData.get(), aData.Length(), &written));

  ASSERT_EQ(aData.Length(), written);
}

void CreateRegisteredDataManager(
    const quota::OriginMetadata& aOriginMetadata,
    Registered<data::FileSystemDataManager>& aRegisteredDataManager) {
  bool done = false;

  data::FileSystemDataManager::GetOrCreateFileSystemDataManager(aOriginMetadata)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&aRegisteredDataManager,
           &done](Registered<data::FileSystemDataManager>
                      registeredDataManager) mutable {
            auto doneOnReturn = MakeScopeExit([&done]() { done = true; });

            ASSERT_TRUE(registeredDataManager->IsOpen());
            aRegisteredDataManager = std::move(registeredDataManager);
          },
          [&done](nsresult rejectValue) {
            auto doneOnReturn = MakeScopeExit([&done]() { done = true; });

            ASSERT_NSEQ(NS_OK, rejectValue);
          });

  SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

  ASSERT_TRUE(aRegisteredDataManager);
  ASSERT_TRUE(aRegisteredDataManager->IsOpen());
  ASSERT_TRUE(aRegisteredDataManager->MutableDatabaseManagerPtr());
}

void GetUsageValue(const quota::UsageInfo& aUsage, uint64_t& aValue) {
  auto dbUsage = aUsage.DatabaseUsage();
  ASSERT_TRUE(dbUsage.isSome());
  aValue = dbUsage.value();
}

void CheckUsageIsNothing(const quota::UsageInfo& aUsage) {
  EXPECT_TRUE(aUsage.FileUsage().isNothing());
  auto dbUsage = aUsage.DatabaseUsage();
  ASSERT_TRUE(dbUsage.isNothing());
}

void CheckUsageEqualTo(const quota::UsageInfo& aUsage, uint64_t aExpected) {
  EXPECT_TRUE(aUsage.FileUsage().isNothing());
  auto dbUsage = aUsage.DatabaseUsage();
  ASSERT_TRUE(dbUsage.isSome());
  const auto actual = dbUsage.value();
  ASSERT_EQ(actual, aExpected);
}

void CheckUsageGreaterThan(const quota::UsageInfo& aUsage, uint64_t aExpected) {
  EXPECT_TRUE(aUsage.FileUsage().isNothing());
  auto dbUsage = aUsage.DatabaseUsage();
  ASSERT_TRUE(dbUsage.isSome());
  const auto actual = dbUsage.value();
  ASSERT_GT(actual, aExpected);
}

}  // namespace mozilla::dom::fs::test
