/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CONSTANTS_H_
#define DOM_QUOTA_CONSTANTS_H_

#include "nsLiteralString.h"

// The name of the file that we use to load/save the last access time of an
// origin.
// XXX We should get rid of old metadata files at some point, bug 1343576.
#define METADATA_FILE_NAME u".metadata"
#define METADATA_TMP_FILE_NAME u".metadata-tmp"
#define METADATA_V2_FILE_NAME u".metadata-v2"
#define METADATA_V2_TMP_FILE_NAME u".metadata-v2-tmp"

namespace mozilla::dom::quota {

const char kChromeOrigin[] = "chrome";

constexpr auto kSQLiteSuffix = u".sqlite"_ns;

constexpr nsLiteralCString kUUIDOriginScheme = "uuid"_ns;

// Special value used when quota version is unknown or not applicable.
//
// This is used in the following cases:
// - When loading quota info from the L1 cache (see LoadQuota)
// - When reading a metadata file that hasn't yet been upgraded to include the
//   quota version field
// - When the metadata file is missing or corrupted and must be restored
//
// In these situations, the quota version is effectively undefined and cannot
// be trusted.
const uint32_t kNoQuotaVersion = 0;

// Current version of the quota management.
//
// This value is written to disk when a metadata file is created for a new
// (empty) origin directory or after performing a full origin directory scan.
// It represents the version of quota tracking logic used to generate the
// originUsage and clientUsages values.
//
// The version must be incremented whenever the quota management logic changes
// in a way that could invalidate existing cached usage data. This includes:
// - Adding a new quota client
// - Removing an existing quota client
// - Changing how usage is calculated or stored
// - Any other change that could cause a mismatch between actual on-disk usage
//   and the cached originUsage/clientUsages values
//
// At present, it is the responsibility of patch authors and reviewers to
// decide when a bump is required. However, in the future, a test will verify
// the correctness of cached usage data by comparing it against real usage,
// using a pre-packaged or conditioned profile.
//
// If you're unsure whether a bump is needed, it's safer to do one. However,
// keep in mind that increasing this version will invalidate the L2 quota info
// cache. When the L1 quota info cache can't be used, such as when the build ID
// changes, after a crash, or on Android in general, and the L2 quota info
// cache is also unavailable due to the version bump, storage initialization
// will have to fall back to the slowest path: a full storage scan.
const uint32_t kCurrentQuotaVersion = 1;

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CONSTANTS_H_
