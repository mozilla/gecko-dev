/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCKINLINES_H_
#define DOM_QUOTA_DIRECTORYLOCKINLINES_H_

#ifndef DOM_QUOTA_DIRECTORYLOCK_H_
#  error Must include DirectoryLock.h first
#endif

#include "mozilla/RefPtr.h"

namespace mozilla::dom::quota {

template <typename T>
constexpr void SafeDropDirectoryLock(RefPtr<T>& aDirectoryLock) {
  if (!aDirectoryLock) {
    return;
  }

  aDirectoryLock->Drop();
  aDirectoryLock = nullptr;
}

template <typename T>
constexpr void DropDirectoryLock(RefPtr<T>& aDirectoryLock) {
  MOZ_ASSERT(aDirectoryLock);

  aDirectoryLock->Drop();
  aDirectoryLock = nullptr;
}

template <typename T>
constexpr void SafeDropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock) {
  if (!aDirectoryLock) {
    return;
  }

  if (!aDirectoryLock->Dropped()) {
    aDirectoryLock->Drop();
  }
  aDirectoryLock = nullptr;
}

template <typename T>
constexpr void DropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock) {
  MOZ_ASSERT(aDirectoryLock);

  if (!aDirectoryLock->Dropped()) {
    aDirectoryLock->Drop();
  }
  aDirectoryLock = nullptr;
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCKINLINES_H_
