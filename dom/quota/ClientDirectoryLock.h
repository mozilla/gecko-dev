/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CLIENTDIRECTORYLOCK_H_
#define DOM_QUOTA_CLIENTDIRECTORYLOCK_H_

#include "nsStringFwd.h"
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

enum class DirectoryLockCategory : uint8_t;
struct OriginMetadata;
class OriginScope;
class PersistenceScope;
class QuotaManager;
class UniversalDirectoryLock;

// A directory lock specialized for a given client directory (inside an origin
// directory).
class ClientDirectoryLock final : public DirectoryLockImpl {
  friend class QuotaManager;
  friend class UniversalDirectoryLock;

 public:
  using DirectoryLockImpl::DirectoryLockImpl;

  // XXX These getters shouldn't exist in the base class, but since some
  // consumers don't use proper casting to ClientDirectoryLock yet, we keep
  // them in the base class and have explicit forwarding here.

  // 'Get' prefix is to avoid name collisions with the enum
  PersistenceType GetPersistenceType() const {
    return DirectoryLockImpl::GetPersistenceType();
  }

  quota::OriginMetadata OriginMetadata() const {
    return DirectoryLockImpl::OriginMetadata();
  }

  const nsACString& Origin() const { return DirectoryLockImpl::Origin(); }

  Client::Type ClientType() const { return DirectoryLockImpl::ClientType(); }

 private:
  static RefPtr<ClientDirectoryLock> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata, Client::Type aClientType,
      bool aExclusive);

  static RefPtr<ClientDirectoryLock> Create(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      const PersistenceScope& aPersistenceScope,
      const OriginScope& aOriginScope,
      const Nullable<Client::Type>& aClientType, bool aExclusive,
      bool aInternal, ShouldUpdateLockIdTableFlag aShouldUpdateLockIdTableFlag,
      DirectoryLockCategory aCategory);
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CLIENTDIRECTORYLOCK_H_
