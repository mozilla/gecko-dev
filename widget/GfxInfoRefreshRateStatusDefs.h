/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

GFXINFO_REFRESH_RATE_STATUS(Any, "ANY")
GFXINFO_REFRESH_RATE_STATUS(AnySame, "ANY_SAME")
GFXINFO_REFRESH_RATE_STATUS(Single, "SINGLE")
GFXINFO_REFRESH_RATE_STATUS(MultipleSame, "MULTIPLE_SAME")
GFXINFO_REFRESH_RATE_STATUS(Mixed, "MIXED")
