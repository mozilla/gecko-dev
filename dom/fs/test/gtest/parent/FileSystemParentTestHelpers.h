/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_PARENT_FILESYSTEMPARENTTESTHELPERS_H_
#define DOM_FS_TEST_GTEST_PARENT_FILESYSTEMPARENTTESTHELPERS_H_

#include "mozilla/dom/FileSystemTypes.h"

namespace mozilla::dom {

namespace quota {

struct OriginMetadata;
class UsageInfo;

}  // namespace quota

namespace fs {

class FileSystemChildMetadata;
template <class T>
class Registered;

namespace data {

class FileSystemDatabaseManager;
class FileSystemDataManager;

}  // namespace data
}  // namespace fs
}  // namespace mozilla::dom

namespace mozilla::dom::fs::test {

int GetPageSize();

const Name& GetTestFileName();

uint64_t BytesOfName(const Name& aName);

const nsCString& GetTestData();

void CreateNewEmptyFile(data::FileSystemDatabaseManager* const aDatabaseManager,
                        const FileSystemChildMetadata& aFileSlot,
                        EntryId& aEntryId);

void WriteDataToFile(const quota::OriginMetadata& aOriginMetadata,
                     data::FileSystemDatabaseManager* const aDatabaseManager,
                     const EntryId& aEntryId, const nsCString& aData);

void CreateRegisteredDataManager(
    const quota::OriginMetadata& aOriginMetadata,
    Registered<data::FileSystemDataManager>& aRegisteredDataManager);

void GetUsageValue(const quota::UsageInfo& aUsage, uint64_t& aValue);

void CheckUsageIsNothing(const quota::UsageInfo& aUsage);

void CheckUsageEqualTo(const quota::UsageInfo& aUsage, uint64_t aExpected);

void CheckUsageGreaterThan(const quota::UsageInfo& aUsage, uint64_t aExpected);

}  // namespace mozilla::dom::fs::test

#endif  // DOM_FS_TEST_GTEST_PARENT_PARENT_FILESYSTEMPARENTTESTHELPERS_H_
