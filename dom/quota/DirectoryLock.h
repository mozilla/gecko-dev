/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCK_H_
#define DOM_QUOTA_DIRECTORYLOCK_H_

template <typename T, typename Serialized>
class EnumSet;

template <class T>
class RefPtr;

namespace mozilla::dom::quota {

enum class DirectoryLockCategory : uint8_t;

template <typename T>
constexpr void SafeDropDirectoryLock(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void DropDirectoryLock(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void SafeDropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void DropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock);

inline auto MakeBlockedByChecker(
    const EnumSet<DirectoryLockCategory>& aCategories);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCK_H_
