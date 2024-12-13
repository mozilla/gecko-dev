/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This tests the multiple update downloads per Firefox session feature.
 *
 * This test does some unusual things, compared to the other files in this
 * directory. We want to start the updates with aus.checkForBackgroundUpdates()
 * to ensure that we test the whole flow. Other tests start update with things
 * like aus.downloadUpdate(), but that bypasses some of the exact checks that we
 * are trying to test as part of the multiupdate flow.
 *
 * In order to accomplish all this, we will be using app_update.sjs to serve
 * updates XMLs and MARs. Outside of this test, this is really only done
 * by browser-chrome mochitests (in ../browser). So we have to do some weird
 * things to make it work properly in an xpcshell test. Things like
 * defining URL_HTTP_UPDATE_SJS in testConstants.js so that it can be read by
 * app_update.sjs in order to provide the correct download URL for MARs, but
 * not reading that file here, because URL_HTTP_UPDATE_SJS is already defined
 * (as something else) in xpcshellUtilsAUS.js.
 */

const FIRST_UPDATE_VERSION = "999998.0";
const SECOND_UPDATE_VERSION = "999999.0";

/**
 * This is like downloadUpdate. The difference is that downloadUpdate assumes
 * that an update actually will be downloaded. This function instead verifies
 * that we the update wasn't downloaded.
 *
 * downloadUpdate(), above, uses aus.checkForBackgroundUpdates() to download
 * updates to verify that background updates actually will check for subsequent
 * updates, but this function will use some slightly different mechanisms. We
 * can call aus.downloadUpdate() and check its return value to see if it started
 * a download. But this doesn't properly check that the update-available
 * notification isn't shown. So we will do an additional check where we follow
 * the normal flow a bit more closely by forwarding the results that we got from
 * checkForUpdates() to aus.onCheckComplete() and make sure that the update
 * prompt isn't shown.
 */
async function testUpdateDoesNotDownload() {
  let check = gUpdateChecker.checkForUpdates(gUpdateChecker.BACKGROUND_CHECK);
  let result = await check.result;
  Assert.ok(result.checksAllowed, "Should be able to check for updates");
  Assert.ok(result.succeeded, "Update check should have succeeded");

  Assert.equal(
    result.updates.length,
    1,
    "Should have gotten 1 update in update check"
  );
  let update = result.updates[0];

  let downloadResult = await gAUS.downloadUpdate(update, true);
  Assert.equal(
    downloadResult,
    Ci.nsIApplicationUpdateService.DOWNLOAD_FAILURE_GENERIC,
    "Expected that we would not start downloading an update"
  );

  let updateAvailableObserved = false;
  let observer = (_subject, _topic, _status) => {
    updateAvailableObserved = true;
  };
  Services.obs.addObserver(observer, "update-available");
  await gAUS.onCheckComplete(result);
  Services.obs.removeObserver(observer, "update-available");
  Assert.equal(
    updateAvailableObserved,
    false,
    "update-available notification should not fire if we aren't going to " +
      "download the update."
  );
}

async function testUpdateCheckDoesNotStart() {
  let updateCheckStarted = await gAUS.checkForBackgroundUpdates();
  Assert.equal(
    updateCheckStarted,
    false,
    "Update check should not have started"
  );
}

function prepareToDownloadVersion(version, onlyCompleteMar = false) {
  let updateUrl = `${gURLData}&useSlowDownloadMar=1&useFirstByteEarly=1&appVersion=${version}`;
  if (onlyCompleteMar) {
    updateUrl += "&completePatchOnly=1";
  }
  setUpdateURL(updateUrl);
}

async function multi_update_test(appUpdateAuto) {
  await UpdateUtils.setAppUpdateAutoEnabled(appUpdateAuto);

  prepareToDownloadVersion(FIRST_UPDATE_VERSION);

  await downloadUpdate({
    appUpdateAuto,
    checkWithAUS: true,
    slowDownload: true,
    onDownloadStartCallback: async () => {
      const readyUpdate = await gUpdateManager.getReadyUpdate();
      const downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
      Assert.ok(!readyUpdate, "There should not be a ready update yet");
      Assert.ok(
        !!downloadingUpdate,
        "First update download should be in downloadingUpdate"
      );
      Assert.equal(
        downloadingUpdate.state,
        STATE_DOWNLOADING,
        "downloadingUpdate should be downloading"
      );
      Assert.equal(
        readStatusFile(),
        STATE_DOWNLOADING,
        "Updater state should be downloading"
      );
    },
  });

  let readyUpdate = await gUpdateManager.getReadyUpdate();
  let downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
  Assert.ok(
    !downloadingUpdate,
    "First update download should no longer be in downloadingUpdate"
  );
  Assert.ok(!!readyUpdate, "First update download should be in readyUpdate");
  Assert.equal(
    readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should be pending"
  );
  Assert.equal(
    readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should be pending"
  );

  let existingUpdate = readyUpdate;
  await testUpdateDoesNotDownload();

  readyUpdate = await gUpdateManager.getReadyUpdate();
  Assert.equal(
    readyUpdate,
    existingUpdate,
    "readyUpdate should not have changed when no newer update is available"
  );
  Assert.equal(
    readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should still be pending"
  );
  Assert.equal(
    readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should still be pending"
  );

  // With only a complete update available, we should not download the newer
  // update when we already have an update ready.
  prepareToDownloadVersion(SECOND_UPDATE_VERSION, true);
  await testUpdateDoesNotDownload();

  readyUpdate = await gUpdateManager.getReadyUpdate();
  Assert.equal(
    readyUpdate,
    existingUpdate,
    "readyUpdate should not have changed when no newer partial update is available"
  );
  Assert.equal(
    readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should still be pending"
  );
  Assert.equal(
    readyUpdate.appVersion,
    FIRST_UPDATE_VERSION,
    "readyUpdate version should be match the version of the first update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should still be pending"
  );

  prepareToDownloadVersion(SECOND_UPDATE_VERSION);

  await downloadUpdate({
    appUpdateAuto,
    checkWithAUS: true,
    slowDownload: true,
    onDownloadStartCallback: async () => {
      readyUpdate = await gUpdateManager.getReadyUpdate();
      downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
      Assert.ok(
        !!downloadingUpdate,
        "Second update download should be in downloadingUpdate"
      );
      Assert.equal(
        downloadingUpdate.state,
        STATE_DOWNLOADING,
        "downloadingUpdate should be downloading"
      );
      Assert.ok(
        !!readyUpdate,
        "First update download should still be in readyUpdate"
      );
      Assert.equal(
        readyUpdate.state,
        STATE_PENDING,
        "readyUpdate should still be pending"
      );
      Assert.equal(
        readyUpdate.appVersion,
        FIRST_UPDATE_VERSION,
        "readyUpdate version should be match the version of the first update"
      );
      Assert.equal(
        readStatusFile(),
        STATE_PENDING,
        "Updater state should match the readyUpdate's state"
      );
    },
  });

  readyUpdate = await gUpdateManager.getReadyUpdate();
  downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
  Assert.ok(
    !downloadingUpdate,
    "Second update download should no longer be in downloadingUpdate"
  );
  Assert.ok(!!readyUpdate, "Second update download should be in readyUpdate");
  Assert.equal(
    readyUpdate.state,
    STATE_PENDING,
    "readyUpdate should be pending"
  );
  Assert.equal(
    readyUpdate.appVersion,
    SECOND_UPDATE_VERSION,
    "readyUpdate version should be match the version of the second update"
  );
  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "Updater state should be pending"
  );

  // Reset the updater to its initial state to test that the complete/partial
  // MAR behavior is correct
  await reloadUpdateManagerData(true);

  // Second parameter forces a complete MAR download.
  prepareToDownloadVersion(FIRST_UPDATE_VERSION, true);

  await downloadUpdate({
    appUpdateAuto,
    checkWithAUS: true,
    slowDownload: true,
    onDownloadStartCallback: async () => {
      downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
      Assert.equal(
        downloadingUpdate.selectedPatch.type,
        "complete",
        "First update download should be a complete patch"
      );
    },
  });

  readyUpdate = await gUpdateManager.getReadyUpdate();
  Assert.equal(
    readyUpdate.selectedPatch.type,
    "complete",
    "First update download should be a complete patch"
  );

  // Even a newer partial update should not be downloaded at this point.
  prepareToDownloadVersion(SECOND_UPDATE_VERSION);
  await testUpdateCheckDoesNotStart();
}

add_task(async function all_multi_update_tests() {
  setupTestCommon(true);
  startSjsServer();

  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);

  let origAppUpdateAutoVal = await UpdateUtils.getAppUpdateAutoEnabled();
  registerCleanupFunction(async () => {
    await UpdateUtils.setAppUpdateAutoEnabled(origAppUpdateAutoVal);
  });

  await multi_update_test(true);

  // Reset the update system so we can start again from scratch.
  await reloadUpdateManagerData(true);

  await multi_update_test(false);

  await doTestFinish();
});
