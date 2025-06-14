/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_DIRECTORYMETADATA_H_
#define DOM_QUOTA_DIRECTORYMETADATA_H_

#include <cstdint>

enum class nsresult : uint32_t;

class nsIBinaryInputStream;
class nsIBinaryOutputStream;
class nsIFile;

namespace mozilla {

template <typename V, typename E>
class Result;

}

namespace mozilla::dom::quota {

struct OriginStateMetadata;

/**
 * Directory Metadata File Format (.metadata-v2)
 *
 * The metadata file is a binary file containing metadata information for an
 * origin directory. It consists of a header and several additional fields,
 * some of which are maintained only for backward compatibility.
 *
 * Header (OriginStateMetadata):
 * - int64_t mLastAccessTime
 *     The last access time of the origin in microseconds since the epoch.
 * - bool mPersisted
 *     True if the origin is marked as persisted and should survive origin
 *     eviction.
 * - uint32_t flags
 *     A bitfield of DirectoryMetadataFlags used to store boolean state flags.
 *     This field currently maps only to mAccessed. The defined flags are:
 *       - Initialized: Always set when writing metadata; indicates that this
 *         field contains valid flag bits. Older files written before this
 *         flag was introduced will have this field set to zero.
 *       - Accessed: Indicates whether the origin has been accessed by a quota
 *         client. This maps directly to the mAccessed field in memory.
 *
 *     If the Initialized flag is not set, the flags field is considered
 *     invalid and mAccessed is conservatively set to true to ensure a full
 *     initialization scan.
 * - uint32_t reservedData
 *     Reserved for future use. Currently ignored.
 *
 * Legacy fields (still written and read for backward compatibility, but no
 * longer used):
 * - nsCString mSuffix
 *     Originally used for origin attributes. Still written to preserve
 *     compatibility.
 * - nsCString mGroup
 *     Originally used for quota group. Still written to preserve
 *     compatibility.
 *
 * Storage fields:
 * - nsCString mStorageOrigin
 *     Storage origin string (actively used for reconstructing the principal).
 *
 * Legacy fields (continued):
 * - bool mIsPrivate
 *     Flag originally used for private browsing contexts or apps. Still
 *     written.
 *
 * Validation check:
 * - After reading all expected fields, any additional data (even a single
 *   32-bit value) is treated as an error.
 *
 * Notes:
 * - OriginStateMetadata is loaded first and interpreted independently. This
 *   allows fast and safe updates to the metadata header on disk without
 *   rewriting the full file.
 * - The header is intentionally designed to contain only fixed-size fields.
 *   This allows updating the header in-place without creating a temporary
 *   file.
 */

Result<OriginStateMetadata, nsresult> ReadDirectoryMetadataHeader(
    nsIBinaryInputStream& aStream);

nsresult WriteDirectoryMetadataHeader(
    nsIBinaryOutputStream& aStream,
    const OriginStateMetadata& aOriginStateMetadata);

Result<OriginStateMetadata, nsresult> LoadDirectoryMetadataHeader(
    nsIFile& aDirectory);

nsresult SaveDirectoryMetadataHeader(
    nsIFile& aDirectory, const OriginStateMetadata& aOriginStateMetadata);

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_NOTIFYUTILS_H_
