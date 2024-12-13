/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
const syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
  Ci.nsIUpdateSyncManager
);

/**
 * Test the Multi Session Install Lockout feature. This feature is meant to
 * prevent an update from being installed at application startup if other
 * instances are already running and a certain time window has not elapsed.
 * Once the timer expires, updates will be installed as usual.
 */

async function testMsil({
  msilEnabled,
  msilTimeoutHr,
  installAttemptDelayHr,
  otherInstancesRunning,
}) {
  const msilTimeoutMs = msilTimeoutHr * 1000 * 60 * 60;
  const installAttemptDelayMs = installAttemptDelayHr * 1000 * 60 * 60;

  Services.prefs.setBoolPref(
    PREF_APP_UPDATE_INSTALL_LOCKOUT_ENABLED,
    msilEnabled
  );
  Services.prefs.setIntPref(
    PREF_APP_UPDATE_INSTALL_LOCKOUT_TIMEOUT_MS,
    msilTimeoutMs
  );

  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  await setupUpdaterTest(FILE_COMPLETE_MAR, false, "", false);

  const preDownloadTimestamp = Date.now();
  await downloadUpdate({ checkWithAUS: true });
  const postDownloadTimestamp = Date.now();

  let msilTimestampMs = readMsilTimeoutFile();
  if (msilEnabled && msilTimeoutMs > 0) {
    const msilTimestampInt = parseInt(msilTimestampMs, 10);
    Assert.greaterOrEqual(
      msilTimestampInt,
      preDownloadTimestamp + msilTimeoutMs
    );
    Assert.lessOrEqual(msilTimestampInt, postDownloadTimestamp + msilTimeoutMs);
  } else {
    Assert.equal(msilTimestampMs, null);
  }

  // Simulate the timeout "counting down" by artificially bumping the timestamp
  // backwards by the delay amount that we are simulating.
  if (msilTimestampMs != null && installAttemptDelayMs > 0) {
    msilTimestampMs = (msilTimestampMs - installAttemptDelayMs).toString();
    writeMsilTimeoutFile(msilTimestampMs);
  }

  let appBin = getApplyDirFile(DIR_MACOS + FILE_APP_BIN);
  if (!otherInstancesRunning) {
    // Use a fake path so that this doesn't show up as another running instance.
    appBin = appBin.parent;
    appBin.append("fakeDir");
    appBin.append("fakeExe");
  }
  syncManager.resetLock(appBin);

  const msilShouldPreventUpdate =
    otherInstancesRunning &&
    msilEnabled &&
    msilTimeoutMs > 0 &&
    installAttemptDelayMs < msilTimeoutMs;

  const expectedState = msilShouldPreventUpdate
    ? STATE_PENDING
    : STATE_SUCCEEDED;
  await runUpdateUsingApp(expectedState);

  if (msilShouldPreventUpdate) {
    // If we did not update, the timestamp file must not have changed so that
    // the next update (within the window) won't be installed either.
    const newMsilTimeoutMs = readMsilTimeoutFile();
    Assert.equal(
      newMsilTimeoutMs,
      msilTimestampMs,
      "Timestamp should not have changed"
    );
  }

  // Reset things for the next test.
  cleanupUpdateFiles();
  await reloadUpdateManagerData(true);
}

add_task(async function test_multi_session_install_lockout() {
  setupTestCommon(true);
  startSjsServer();
  setUpdateURL(gURLData + "&completePatchOnly=1");
  setUpdateChannel("test_channel");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);

  const origAppUpdateAutoVal = await UpdateUtils.getAppUpdateAutoEnabled();
  registerCleanupFunction(async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(origAppUpdateAutoVal);
  });

  await parameterizedTest(testMsil, {
    msilEnabled: [true, false],
    msilTimeoutHr: [0, 24],
    installAttemptDelayHr: [12, 120],
    otherInstancesRunning: [true, false],
  });

  await doTestFinish();
});
