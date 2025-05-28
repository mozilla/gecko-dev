/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/ClientDirectoryLockHandle.h"

#include "mozilla/dom/quota/ClientDirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/DirectoryLockInlines.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla::dom::quota {

ClientDirectoryLockHandle::ClientDirectoryLockHandle() {
  MOZ_COUNT_CTOR(mozilla::dom::quota::ClientDirectoryLockHandle);
}

ClientDirectoryLockHandle::ClientDirectoryLockHandle(
    RefPtr<ClientDirectoryLock> aClientDirectoryLock) {
  aClientDirectoryLock->AssertIsOnOwningThread();

  mClientDirectoryLock = std::move(aClientDirectoryLock);

  MOZ_COUNT_CTOR(mozilla::dom::quota::ClientDirectoryLockHandle);
}

ClientDirectoryLockHandle::ClientDirectoryLockHandle(
    ClientDirectoryLockHandle&& aOther) noexcept {
  aOther.AssertIsOnOwningThread();

  mClientDirectoryLock = std::move(aOther.mClientDirectoryLock);

  // Explicitly null aOther.mClientDirectoryLock so that aOther appears inert
  // immediately after the move. While RefPtr nulls out its mRawPtr internally,
  // the store may be reordered in optimized builds, possibly occurring only
  // just before RefPtr’s own destructor runs. Without this, the moved-from
  // handle’s destructor may observe a stale non-null value.
  aOther.mClientDirectoryLock = nullptr;

  mRegistered = std::exchange(aOther.mRegistered, false);

  MOZ_COUNT_CTOR(mozilla::dom::quota::ClientDirectoryLockHandle);
}

ClientDirectoryLockHandle::~ClientDirectoryLockHandle() {
  // If mClientDirectoryLock is null, this handle is in an inert state — either
  // it was default-constructed or moved.
  //
  // This check is safe here because destruction implies no other thread is
  // using the handle. Any use-after-destroy bugs would indicate a much more
  // serious issue (e.g., a dangling pointer), and should be caught by tools
  // like AddressSanitizer.
  if (mClientDirectoryLock) {
    AssertIsOnOwningThread();

    mClientDirectoryLock->MutableManagerRef().ClientDirectoryLockHandleDestroy(
        *this);

    DropDirectoryLock(mClientDirectoryLock);
  }

  MOZ_COUNT_DTOR(mozilla::dom::quota::ClientDirectoryLockHandle);
}

void ClientDirectoryLockHandle::AssertIsOnOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(ClientDirectoryLockHandle);
}

ClientDirectoryLockHandle& ClientDirectoryLockHandle::operator=(
    ClientDirectoryLockHandle&& aOther) noexcept {
  AssertIsOnOwningThread();
  aOther.AssertIsOnOwningThread();

  if (this != &aOther) {
    mClientDirectoryLock = std::move(aOther.mClientDirectoryLock);

    // Explicitly null aOther.mClientDirectoryLock so that aOther appears inert
    // immediately after the move. While RefPtr nulls out its mRawPtr
    // internally, the store may be reordered in optimized builds, possibly
    // occurring only just before RefPtr’s own destructor runs. Without this,
    // the moved-from handle’s destructor may observe a stale non-null value.
    aOther.mClientDirectoryLock = nullptr;

    mRegistered = std::exchange(aOther.mRegistered, false);
  }

  return *this;
}

ClientDirectoryLockHandle::operator bool() const {
  AssertIsOnOwningThread();

  return !!mClientDirectoryLock;
}

ClientDirectoryLock* ClientDirectoryLockHandle::get() const {
  AssertIsOnOwningThread();

  return mClientDirectoryLock ? mClientDirectoryLock.get() : nullptr;
}

ClientDirectoryLock& ClientDirectoryLockHandle::operator*() const {
  AssertIsOnOwningThread();

  return *get();
}

ClientDirectoryLock* ClientDirectoryLockHandle::operator->() const {
  AssertIsOnOwningThread();

  return get();
}

bool ClientDirectoryLockHandle::IsRegistered() const {
  AssertIsOnOwningThread();

  return mRegistered;
}

void ClientDirectoryLockHandle::SetRegistered(bool aRegistered) {
  AssertIsOnOwningThread();

  mRegistered = aRegistered;
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

bool ClientDirectoryLockHandle::IsInert() const {
  return !mClientDirectoryLock;
}

#endif

}  // namespace mozilla::dom::quota
