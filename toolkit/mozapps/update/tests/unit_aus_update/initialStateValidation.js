/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This test ensures that if we start up in a good state, we use that state. But
 * if we start out in a bad/inconsistent state, we toss that state out and start
 * fresh.
 */

let gServerPort;

// enum of possible states we might want to put the update object into when
// testing.
const UpdateObjectState = {
  nonexistent: "UpdateObjectState::nonexistent",
  hasWrongState: "UpdateObjectState::hasWrongState",
  expected: "UpdateObjectState::expected",
};

async function testInitialStateValidation({
  updateState,
  downloadingUpdateState,
  readyUpdateState,
  readyUpdateMarExists,
}) {
  const url =
    APP_UPDATE_SJS_HOST +
    ":" +
    gServerPort +
    "/" +
    REL_PATH_DATA +
    "app_update.sjs?slowDownloadMar=1";
  const statusFile = getUpdateDirFile(FILE_UPDATE_STATUS);
  const updateXmlFile = getUpdateDirFile(FILE_ACTIVE_UPDATE_XML);
  const readyMarFile = getUpdateDirFile(FILE_UPDATE_MAR, DIR_PATCH);
  const downloadingMarFile = getUpdateDirFile(FILE_UPDATE_MAR, DIR_DOWNLOADING);

  // Step 1: Write the requested state to disk.
  let statusFileState = updateState;
  if (statusFileState == STATE_FAILED) {
    // If the state is failed, the status file is expected to have some sort of
    // error code, but which one specifically isn't especially important for
    // this test.
    statusFileState += STATE_FAILED_DELIMETER;
    statusFileState += READ_ERROR;
  }
  writeStatusFile(statusFileState);

  let updates = "";
  let readyUpdateStatus;
  if (readyUpdateState != UpdateObjectState.nonexistent) {
    readyUpdateStatus = updateState;
    if (downloadingUpdateState == UpdateObjectState.hasWrongState) {
      readyUpdateStatus = STATE_NONE;
    }
    const patches = getLocalPatchString({ state: readyUpdateStatus, url });
    updates += getLocalUpdateString({ appVersion: "2" }, patches);
  }
  let downloadingUpdateStatus;
  if (downloadingUpdateState != UpdateObjectState.nonexistent) {
    downloadingUpdateStatus = STATE_DOWNLOADING;
    if (downloadingUpdateState == UpdateObjectState.hasWrongState) {
      downloadingUpdateStatus = STATE_NONE;
    }
    const patches = getLocalPatchString({
      state: downloadingUpdateStatus,
      url,
    });
    updates += getLocalUpdateString({ appVersion: "3" }, patches);
  }

  if (updates.length) {
    writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  } else {
    ensureRemoved(updateXmlFile);
  }

  if (readyUpdateMarExists) {
    writeFile(readyMarFile, "test mar contents");
  } else {
    ensureRemoved(readyMarFile);
  }

  // Step 2: Reload the update state from the disk.
  await reloadUpdateManagerData();
  await reInitUpdateService();

  // Step 3: Figure out what the current state _ought_ to be.
  let expectedAusState;
  switch (updateState) {
    case STATE_DOWNLOADING:
      if (
        downloadingUpdateState == UpdateObjectState.nonexistent ||
        readyUpdateState != UpdateObjectState.nonexistent
      ) {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      } else {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_DOWNLOADING;
      }
      break;
    case STATE_PENDING:
    case STATE_PENDING_SVC:
    case STATE_APPLIED:
    case STATE_APPLIED_SVC:
      if (
        readyUpdateState == UpdateObjectState.nonexistent ||
        !readyUpdateMarExists
      ) {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      } else {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_PENDING;
        readyUpdateStatus = updateState;
      }
      break;
    case STATE_APPLYING:
      // We don't have a testcase where the status file has state applying and
      // the ready update state is pending or pending-service, which is the
      // only case where we actually want to keep that update around rather
      // than cleaning it up. But we do want to revert to the downloading state
      // if there is a download in-progress.
      if (
        downloadingUpdateState == UpdateObjectState.expected &&
        readyUpdateMarExists
      ) {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_DOWNLOADING;
      } else {
        expectedAusState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      }
      break;
    default:
      expectedAusState = Ci.nsIApplicationUpdateService.STATE_IDLE;
      break;
  }

  let expectReadyUpdatePresent;
  let expectDownloadingUpdatePresent;
  switch (expectedAusState) {
    case Ci.nsIApplicationUpdateService.STATE_IDLE:
      expectReadyUpdatePresent = false;
      expectDownloadingUpdatePresent = false;
      break;
    case Ci.nsIApplicationUpdateService.STATE_DOWNLOADING:
      expectReadyUpdatePresent = false;
      expectDownloadingUpdatePresent =
        downloadingUpdateState == UpdateObjectState.expected;
      break;
    case Ci.nsIApplicationUpdateService.STATE_PENDING:
      expectReadyUpdatePresent = true;
      expectDownloadingUpdatePresent =
        downloadingUpdateState == UpdateObjectState.expected;
      break;
    default:
      // We always assign one of three states above. It should be impossible to
      // hit this branch.
      Assert.ok(
        false,
        `Unexpected value of expectedAusState=${expectedAusState}`
      );
      break;
  }

  // Step 3: Verify that the actual state matches the expected state.
  Assert.equal(
    gAUS.currentState,
    expectedAusState,
    `AUS state - actual=${gAUS.getStateName(gAUS.currentState)} expected=${gAUS.getStateName(expectedAusState)}`
  );
  const readyUpdate = await gUpdateManager.getReadyUpdate();
  const downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
  Assert.equal(
    !!readyUpdate,
    expectReadyUpdatePresent,
    `readyUpdate should${expectReadyUpdatePresent ? "" : " not"} be present: ${readyUpdate}`
  );
  Assert.equal(
    !!downloadingUpdate,
    expectDownloadingUpdatePresent,
    `downloadingUpdate should${expectDownloadingUpdatePresent ? "" : " not"} be present`
  );
  if (expectReadyUpdatePresent) {
    Assert.equal(
      readyUpdate.state,
      readyUpdateStatus,
      "readyUpdate should have the expected state"
    );
  }
  if (expectDownloadingUpdatePresent) {
    Assert.equal(
      downloadingUpdate.state,
      downloadingUpdateStatus,
      "downloadingUpdate should have the expected state"
    );
  }

  // Step 4: Clean up.
  if (expectDownloadingUpdatePresent) {
    // It's a bit of a problem to clean up an in-progress download in a reliable
    // way. Let's just let it finish downloading before we try to move on.
    let downloadFinished = gAUS.stateTransition;
    await continueFileHandler(CONTINUE_DOWNLOAD);
    await downloadFinished;
    // We could have just transitioned from downloading to pending or from
    // pending to swap. If we are in swap state, wait for that to finish too.
    if (gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_SWAP) {
      await gAUS.stateTransition;
    }
  }
  ensureRemoved(statusFile);
  ensureRemoved(updateXmlFile);
  ensureRemoved(readyMarFile);
  ensureRemoved(downloadingMarFile);
}

add_task(async function initialStateValidation() {
  setupTestCommon(true);
  const server = startSjsServer();
  gServerPort = server.identity.primaryPort;
  setUpdateChannel("test_channel");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);

  await parameterizedTest(
    testInitialStateValidation,
    {
      updateState: [
        STATE_NONE,
        STATE_DOWNLOADING,
        STATE_PENDING,
        STATE_PENDING_SVC,
        STATE_APPLYING,
        STATE_APPLIED,
        STATE_APPLIED_SVC,
        STATE_SUCCEEDED,
        STATE_DOWNLOAD_FAILED,
        STATE_FAILED,
        "bad-state",
      ],
      downloadingUpdateState: Object.values(UpdateObjectState),
      readyUpdateState: Object.values(UpdateObjectState),
      readyUpdateMarExists: [true, false],
    },
    {
      skipFn: ({ updateState, downloadingUpdateState, readyUpdateState }) => {
        // We don't actually store our updates in a way where, if there is only a
        // single update object, it is inherently clear which one it is meant to be.
        // We figure this out by inspecting the state. So a single update in a
        // non-downloading state will always be interpreted as the ready update.
        // We are going to skip these test cases because it is expected that they
        // would fail.
        return (
          (readyUpdateState == UpdateObjectState.nonexistent &&
            (downloadingUpdateState == UpdateObjectState.hasWrongState ||
              updateState != STATE_DOWNLOADING)) ||
          // Similar to the above situation, the ready update will be misconstrued as
          // the downloading update in this situation.
          (updateState == STATE_DOWNLOADING &&
            readyUpdateState != UpdateObjectState.nonexistent &&
            downloadingUpdateState == UpdateObjectState.nonexistent)
        );
      },
    }
  );

  await doTestFinish();
});
