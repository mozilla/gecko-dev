/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacUtils_h_
#define MacUtils_h_

#include <Cocoa/Cocoa.h>

namespace mozilla::MacUtils {

void LaunchTask(NSString* aPath, NSArray* aArguments);

}  // namespace mozilla::MacUtils

#endif
