/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// driver <  version
GFXINFO_DRIVER_VERSION_CMP(LESS_THAN)
// driver build id <  version
GFXINFO_DRIVER_VERSION_CMP(BUILD_ID_LESS_THAN)
// driver <= version
GFXINFO_DRIVER_VERSION_CMP(LESS_THAN_OR_EQUAL)
// driver build id <= version
GFXINFO_DRIVER_VERSION_CMP(BUILD_ID_LESS_THAN_OR_EQUAL)
// driver >  version
GFXINFO_DRIVER_VERSION_CMP(GREATER_THAN)
// driver >= version
GFXINFO_DRIVER_VERSION_CMP(GREATER_THAN_OR_EQUAL)
// driver == version
GFXINFO_DRIVER_VERSION_CMP(EQUAL)
// driver != version
GFXINFO_DRIVER_VERSION_CMP(NOT_EQUAL)
// driver > version && driver < versionMax
GFXINFO_DRIVER_VERSION_CMP(BETWEEN_EXCLUSIVE)
// driver >= version && driver <= versionMax
GFXINFO_DRIVER_VERSION_CMP(BETWEEN_INCLUSIVE)
// driver >= version && driver < versionMax
GFXINFO_DRIVER_VERSION_CMP(BETWEEN_INCLUSIVE_START)
// do not compare driver versions
GFXINFO_DRIVER_VERSION_CMP(COMPARISON_IGNORED)
