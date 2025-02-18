/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This namespace contains methods with Obj-C implementations for Apple
// platforms. The header is C/C++ for inclusion in C/C++-only files.

#ifndef AppleFileUtils_h_
#define AppleFileUtils_h_

#include "nsString.h"

namespace DarwinFileUtils {

void GetTemporaryDirectory(nsACString& aFilePath);

}  // namespace DarwinFileUtils

#endif
