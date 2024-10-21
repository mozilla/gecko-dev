/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_UNIVERSALDIRECTORYLOCK_H_
#define DOM_QUOTA_UNIVERSALDIRECTORYLOCK_H_

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/DirectoryLockImpl.h"
#include "mozilla/dom/quota/PersistenceType.h"

template <class T>
class RefPtr;

namespace mozilla {

template <typename T>
class MovingNotNull;

}  // namespace mozilla

namespace mozilla::dom {

template <typename T>
struct Nullable;

}  // namespace mozilla::dom

namespace mozilla::dom::quota {

class ClientDirectoruLock;
enum class DirectoryLockCategory : uint8_t;
struct OriginMetadata;
class OriginScope;
class PersistenceScope;
class QuotaManager;

// A directory lock for universal use. A universal lock can handle any possible
// combination of nullable persistence type, origin scope and nullable client
// type.
//
// For example, if the persistence type is set to null, origin scope is null
// and the client type is set to Client::IDB, then the lock will cover
// <profile>/storage/*/*/idb
//
// If no property is set, then the lock will cover the entire storage directory
// and its subdirectories.
class UniversalDirectoryLock final : public DirectoryLockImpl {
  friend class QuotaManager;

 public:
  using DirectoryLockImpl::DirectoryLockImpl;

  RefPtr<ClientDirectoryLock> SpecializeForClient(
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata,
      Client::Type aClientType) const;

 private:
  static RefPtr<UniversalDirectoryLock> CreateInternal(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PersistenceScope& aPersistenceScope,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      DirectoryLockCategory aCategory);
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_UNIVERSALDIRECTORYLOCK_H_
