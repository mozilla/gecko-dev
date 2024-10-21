/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINDIRECTORYLOCK_H_
#define DOM_QUOTA_ORIGINDIRECTORYLOCK_H_

#include "nsStringFwd.h"
#include "mozilla/dom/quota/DirectoryLockImpl.h"
#include "mozilla/dom/quota/PersistenceType.h"

template <class T>
class RefPtr;

namespace mozilla {

template <typename T>
class MovingNotNull;

}  // namespace mozilla

namespace mozilla::dom::quota {

struct OriginMetadata;
class QuotaManager;

// A directory lock specialized for a given origin directory.
class OriginDirectoryLock : public DirectoryLockImpl {
  friend class QuotaManager;

 public:
  using DirectoryLockImpl::DirectoryLockImpl;

  // XXX These getters shouldn't exist in the base class, but since some
  // consumers don't use proper casting to OriginDirectoryLock yet, we keep
  // them in the base class and have explicit forwarding here.

  // 'Get' prefix is to avoid name collisions with the enum
  PersistenceType GetPersistenceType() const {
    return DirectoryLockImpl::GetPersistenceType();
  }

  quota::OriginMetadata OriginMetadata() const {
    return DirectoryLockImpl::OriginMetadata();
  }

  const nsACString& Origin() const { return DirectoryLockImpl::Origin(); }

 private:
  static RefPtr<OriginDirectoryLock> CreateForEviction(
      MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
      PersistenceType aPersistenceType,
      const quota::OriginMetadata& aOriginMetadata);
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINDIRECTORYLOCK_H_
