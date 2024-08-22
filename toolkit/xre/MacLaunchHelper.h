/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacLaunchHelper_h_
#define MacLaunchHelper_h_

#include <stdint.h>
#include <unistd.h>

#ifdef __OBJC__
#  include <Foundation/Foundation.h>

namespace mozilla::MacLaunchHelper {

void LaunchMacAppWithBundle(NSString* aBundlePath,
                            NSArray* aArguments = nullptr);

}  // namespace mozilla::MacLaunchHelper
#endif  // __OBJC__

extern "C" {
/**
 * Passing an aPid parameter to LaunchChildMac will wait for the launched
 * process to terminate. When the process terminates, aPid will be set to the
 * pid of the terminated process to confirm that it executed successfully.
 */
void LaunchChildMac(int aArgc, char** aArgv, pid_t* aPid = 0);
void LaunchMacApp(int aArgc, char** aArgv);
bool LaunchElevatedUpdate(int aArgc, char** aArgv, pid_t* aPid = 0);
bool InstallPrivilegedHelper();
void AbortElevatedUpdate();
}

#endif
