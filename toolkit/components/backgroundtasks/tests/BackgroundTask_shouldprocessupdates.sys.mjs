/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export async function runBackgroundTask() {
  const gleanRoot = await IOUtils.getDirectory(
    Services.dirsvc.get("UpdRootD", Ci.nsIFile).path,
    "backgroundupdate",
    "datareporting",
    "glean"
  );
  Services.prefs.setIntPref("telemetry.fog.test.localhost_port", -1);
  Services.fog.initializeFOG(
    gleanRoot.path,
    "firefox.desktop.background.update.test"
  );

  const get = Services.env.get("MOZ_TEST_SHOULD_NOT_PROCESS_UPDATES");
  let exitCode = 81;
  if (get == "ShouldNotProcessUpdates(): OtherInstanceRunning") {
    if (
      Glean.update.skipStartupUpdateReason.OtherInstanceRunning.testGetValue() >
      0
    ) {
      exitCode = 80;
    } else {
      exitCode = 82;
    }
  }
  if (get == "ShouldNotProcessUpdates(): DevToolsLaunching") {
    if (
      Glean.update.skipStartupUpdateReason.DevToolsLaunching.testGetValue() > 0
    ) {
      exitCode = 79;
    } else {
      exitCode = 82;
    }
  }
  if (get == "ShouldNotProcessUpdates(): NotAnUpdatingTask") {
    if (
      Glean.update.skipStartupUpdateReason.NotAnUpdatingTask.testGetValue() > 0
    ) {
      exitCode = 78;
    } else {
      exitCode = 82;
    }
  }
  if (get == "ShouldNotProcessUpdates(): FirstStartup") {
    if (Glean.update.skipStartupUpdateReason.FirstStartup.testGetValue() > 0) {
      exitCode = 77;
    } else {
      exitCode = 82;
    }
  }
  if (get == "ShouldNotProcessUpdates(): MultiSessionInstallLockout") {
    if (
      Glean.update.skipStartupUpdateReason.MultiSessionInstallLockout.testGetValue() >
      0
    ) {
      exitCode = 76;
    } else {
      exitCode = 82;
    }
  }
  console.debug(`runBackgroundTask: shouldprocessupdates`, {
    exists: Services.env.exists("MOZ_TEST_SHOULD_NOT_PROCESS_UPDATES"),
    get,
  });
  console.error(
    `runBackgroundTask: shouldprocessupdates exiting with exitCode ${exitCode}`
  );

  return exitCode;
}
