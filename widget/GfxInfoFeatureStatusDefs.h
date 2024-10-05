/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// Note that the order of these is important. The downloadable blocklist values
// are cached inside of preferences which store the enum value as an integer.
// This means we cannot reorder these statuses and can only append new values to
// the end.

/* The driver is safe to the best of our knowledge */
GFXINFO_FEATURE_STATUS(STATUS_OK)
/* We don't know the status of the feature yet. The analysis probably hasn't finished yet. */
GFXINFO_FEATURE_STATUS(STATUS_UNKNOWN)
/* This feature is blocked on this driver version. Updating driver will typically unblock it. */
GFXINFO_FEATURE_STATUS(BLOCKED_DRIVER_VERSION)
/* This feature is blocked on this device, regardless of driver version.
 * Typically means we hit too many driver crashes without a good reason to hope for them to
 * get fixed soon. */
GFXINFO_FEATURE_STATUS(BLOCKED_DEVICE)
/* This feature is available and can be used, but is not suggested (e.g. shouldn't be used by default */
GFXINFO_FEATURE_STATUS(DISCOURAGED)
/* This feature is blocked on this OS version. */
GFXINFO_FEATURE_STATUS(BLOCKED_OS_VERSION)
/* This feature is blocked because of mismatched driver versions. */
GFXINFO_FEATURE_STATUS(BLOCKED_MISMATCHED_VERSION)
/* This feature is blocked due to not being on the allowlist. */
GFXINFO_FEATURE_STATUS(DENIED)
/* This feature is safe to be on this device due to the allowlist. */
GFXINFO_FEATURE_STATUS(ALLOW_ALWAYS)
/* This feature is safe to be on this device due to the allowlist, depending on qualified/experiment status. */
GFXINFO_FEATURE_STATUS(ALLOW_QUALIFIED)
/* This feature failed in a startup test, e.g. due to a crashing driver. */
GFXINFO_FEATURE_STATUS(BLOCKED_PLATFORM_TEST)
