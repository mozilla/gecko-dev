/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable no-console */

import { EXIT_CODE } from "resource://gre/modules/BackgroundTasksManager.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

// Usage:
// removeProfileFiles rootDirPath localDirPath lockTimeout
//                    arg0        arg1         arg2
export async function runBackgroundTask(commandLine) {
  let rootDir = commandLine.resolveFile(commandLine.getArgument(0));
  let localDir = commandLine.resolveFile(commandLine.getArgument(1));
  let timeout = parseInt(commandLine.getArgument(2));

  try {
    console.log(
      `Removing profile directories '${rootDir.path}' and '${localDir.path}'`
    );

    // We allow up to 180 seconds for the old process to quit and release the
    // profile lock.
    await lazy.ProfileService.removeProfileFilesByPath(
      rootDir,
      localDir,
      timeout
    );
  } catch (e) {
    console.error(`Failed to remove profile directories: ${e}`);
    return EXIT_CODE.EXCEPTION;
  }

  console.log("Profile directories removed.");
  return EXIT_CODE.SUCCESS;
}
