/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYLOCK_H_
#define DOM_QUOTA_DIRECTORYLOCK_H_

#include <cstdint>
#include <functional>

#include "nscore.h"
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/ForwardDecls.h"

template <class T>
class RefPtr;

namespace mozilla::dom {

template <typename T>
struct Nullable;

}  // namespace mozilla::dom

namespace mozilla::dom::quota {

enum class DirectoryLockCategory : uint8_t;
class OriginScope;
class PersistenceScope;

// Basic directory lock interface shared by all other directory lock classes.
// The class must contain pure virtual functions only to avoid problems with
// multiple inheritance.
class NS_NO_VTABLE DirectoryLock {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual int64_t Id() const = 0;

  virtual const PersistenceScope& PersistenceScopeRef() const = 0;

  // XXX Rename to OriginScopeRef.
  virtual const OriginScope& GetOriginScope() const = 0;

  // XXX Rename to NullableClientTypeRef.
  virtual const Nullable<Client::Type>& NullableClientType() const = 0;

  virtual DirectoryLockCategory Category() const = 0;

  virtual bool Acquired() const = 0;

  virtual bool MustWait() const = 0;

  virtual nsTArray<RefPtr<DirectoryLock>> LocksMustWaitFor() const = 0;

  virtual bool Invalidated() const = 0;

  virtual bool Dropped() const = 0;

  virtual RefPtr<BoolPromise> Acquire() = 0;

  virtual void AcquireImmediately() = 0;

  virtual void AssertIsAcquiredExclusively() = 0;

  virtual RefPtr<BoolPromise> Drop() = 0;

  virtual void OnInvalidate(std::function<void()>&& aCallback) = 0;

  virtual void Log() const = 0;
};

template <typename T>
constexpr void SafeDropDirectoryLock(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void DropDirectoryLock(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void SafeDropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock);

template <typename T>
constexpr void DropDirectoryLockIfNotDropped(RefPtr<T>& aDirectoryLock);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_DIRECTORYLOCK_H_
