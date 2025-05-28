/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_OPENCLIENTDIRECTORYINFO_H_
#define DOM_QUOTA_OPENCLIENTDIRECTORYINFO_H_

#include <cstdint>

#include "mozilla/dom/quota/CommonMetadata.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::quota {

/**
 * @class OpenClientDirectoryInfo
 * @brief Tracks the first and last access to an origin directory.
 *
 * OpenClientDirectoryInfo is a lightweight internal helper used to track
 * access to a specific origin directory after a call to
 * QuotaManager::OpenClientDirectory.
 *
 * It keeps a count of active ClientDirectoryLockHandle instances associated
 * with the origin directory and allows the QuotaManager to update the
 * directory’s access time when the first handle is created and when the last
 * one is released.
 *
 * Although this class is currently focused on tracking origin-level access, it
 * may be extended in the future to track finer-grained access to individual
 * client directories as well. The name reflects its connection to the broader
 * OpenClientDirectory mechanism, which is central to how quota clients
 * initiate access to their storage.
 *
 * ## Usage:
 * - Created by QuotaManager::RegisterClientDirectoryLockHandle.
 * - Removed by QuotaManager::UnregisterClientDirectoryLockHandle.
 *
 * ## Lifetime:
 * - Exists only while at least one ClientDirectoryLockHandle is active for the
 *   origin directory.
 *
 * ## Threading:
 * - Must be used only on the thread that created it.
 * - `AssertIsOnOwningThread()` can be used to verify correct usage.
 */
class OpenClientDirectoryInfo {
 public:
  explicit OpenClientDirectoryInfo(const OriginMetadata& aOriginMetadata);

  ~OpenClientDirectoryInfo();

  void AssertIsOnOwningThread() const;

  const OriginMetadata& OriginMetadataRef() const;

  uint64_t ClientDirectoryLockHandleCount() const;

  void IncreaseClientDirectoryLockHandleCount();

  void DecreaseClientDirectoryLockHandleCount();

 private:
  NS_DECL_OWNINGTHREAD

  // XXX This can be removed once QuotaManager::ClearOpenClientDirectoryInfos
  // is removed.
  OriginMetadata mOriginMetadata;

  // Use uint64_t instead of uint32_t for alignment and compatibility:
  // - This member would be 8-byte aligned/padded on 64-bit platforms anyway.
  // - AssertNoOverflow/AssertNoUnderflow currently support only uint64_t.
  uint64_t mClientDirectoryLockHandleCount = 0;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_OPENCLIENTDIRECTORYINFO_H_
