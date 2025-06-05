/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: No include guard.  This is meant to be included to generate different
// code based on how GFXINFO_FEATURE_STATUS is defined, possibly multiple times
// in a single translation unit.

/* clang-format off */

// For historical reasons, "All" in blocklist means "All Windows"
GFXINFO_OS(Windows, "All")
GFXINFO_OS(Windows7, "WINNT 6.1")
GFXINFO_OS(Windows8, "WINNT 6.2")
GFXINFO_OS(Windows8_1, "WINNT 6.3")
GFXINFO_OS(Windows10, "WINNT 10.0")
GFXINFO_OS(RecentWindows10, "WINNT Recent")
GFXINFO_OS(NotRecentWindows10, "WINNT NotRecent")
GFXINFO_OS(Linux, "Linux")
GFXINFO_OS(OSX, "Darwin")
GFXINFO_OS(OSX10_5, "Darwin 9")
GFXINFO_OS(OSX10_6, "Darwin 10")
GFXINFO_OS(OSX10_7, "Darwin 11")
GFXINFO_OS(OSX10_8, "Darwin 12")
GFXINFO_OS(OSX10_9, "Darwin 13")
GFXINFO_OS(OSX10_10, "Darwin 14")
GFXINFO_OS(OSX10_11, "Darwin 15")
GFXINFO_OS(OSX10_12, "Darwin 16")
GFXINFO_OS(OSX10_13, "Darwin 17")
GFXINFO_OS(OSX10_14, "Darwin 18")
GFXINFO_OS(OSX10_15, "Darwin 19")
GFXINFO_OS(OSX11_0, "Darwin 20")
GFXINFO_OS(Android, "Android")
GFXINFO_OS(Ios, "Ios")
