/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacAutoreleasePool.h"
#include "MacUtils.h"

namespace mozilla::MacUtils {

/**
 * Helper to launch macOS tasks via NSTask and wait for the launched task to
 * terminate.
 */
void LaunchTask(NSString* aPath, NSArray* aArguments) {
  MacAutoreleasePool pool;

  @try {
    NSTask* task = [[NSTask alloc] init];
    [task setExecutableURL:[NSURL fileURLWithPath:aPath]];
    if (aArguments) {
      [task setArguments:aArguments];
    }
    [task launchAndReturnError:nil];
    [task waitUntilExit];
    [task release];
  } @catch (NSException* e) {
    NSLog(@"%@: %@", e.name, e.reason);
  }
}

}  // namespace mozilla::MacUtils
