/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/OpenClientDirectoryInfo.h"

#include "mozilla/dom/quota/AssertionsImpl.h"
#include "mozilla/dom/quota/UniversalDirectoryLock.h"

namespace mozilla::dom::quota {

OpenClientDirectoryInfo::OpenClientDirectoryInfo() {
  MOZ_COUNT_CTOR(mozilla::dom::quota::OpenClientDirectoryInfo);
}

OpenClientDirectoryInfo::~OpenClientDirectoryInfo() {
  MOZ_COUNT_DTOR(mozilla::dom::quota::OpenClientDirectoryInfo);
}

void OpenClientDirectoryInfo::AssertIsOnOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(OpenClientDirectoryInfo);
}

void OpenClientDirectoryInfo::SetLastAccessDirectoryLock(
    RefPtr<UniversalDirectoryLock> aLastAccessDirectoryLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLastAccessDirectoryLock);
  MOZ_ASSERT(!mLastAccessDirectoryLock);

  mLastAccessDirectoryLock = std::move(aLastAccessDirectoryLock);
}

bool OpenClientDirectoryInfo::HasLastAccessDirectoryLock() const {
  AssertIsOnOwningThread();

  return mLastAccessDirectoryLock;
}

RefPtr<UniversalDirectoryLock>
OpenClientDirectoryInfo::ForgetLastAccessDirectoryLock() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mLastAccessDirectoryLock);

  return std::move(mLastAccessDirectoryLock);
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
