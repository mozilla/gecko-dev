/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/OpenClientDirectoryInfo.h"

#include "mozilla/dom/quota/AssertionsImpl.h"

namespace mozilla::dom::quota {

OpenClientDirectoryInfo::OpenClientDirectoryInfo(
    const OriginMetadata& aOriginMetadata)
    : mOriginMetadata(aOriginMetadata) {
  MOZ_COUNT_CTOR(mozilla::dom::quota::OpenClientDirectoryInfo);
}

OpenClientDirectoryInfo::~OpenClientDirectoryInfo() {
  MOZ_COUNT_DTOR(mozilla::dom::quota::OpenClientDirectoryInfo);
}

void OpenClientDirectoryInfo::AssertIsOnOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(OpenClientDirectoryInfo);
}

const OriginMetadata& OpenClientDirectoryInfo::OriginMetadataRef() const {
  AssertIsOnOwningThread();

  return mOriginMetadata;
}

uint64_t OpenClientDirectoryInfo::ClientDirectoryLockHandleCount() const {
  AssertIsOnOwningThread();

  return mClientDirectoryLockHandleCount;
}

void OpenClientDirectoryInfo::IncreaseClientDirectoryLockHandleCount() {
  AssertIsOnOwningThread();

  AssertNoOverflow(mClientDirectoryLockHandleCount, 1);
  mClientDirectoryLockHandleCount++;
}

void OpenClientDirectoryInfo::DecreaseClientDirectoryLockHandleCount() {
  AssertIsOnOwningThread();

  AssertNoUnderflow(mClientDirectoryLockHandleCount, 1);
  mClientDirectoryLockHandleCount--;
}

}  // namespace mozilla::dom::quota
