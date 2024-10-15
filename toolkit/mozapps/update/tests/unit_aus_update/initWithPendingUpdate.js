/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This test checks that when we start up with a pending update, we handle that
 * situation correctly. This means cleaning it up only if it is invalid in some
 * way and otherwise leaving it intact to be installed at the next startup.
 */

async function testInitWithPendingUpdate({ state, useValidMar }) {
  writeStatusFile(state);

  const marFile = getUpdateDirFile(FILE_UPDATE_MAR);
  if (useValidMar) {
    writeFile(marFile, "");
  } else {
    try {
      marFile.remove(false);
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw ex;
      }
    }
  }

  const patches = getLocalPatchString({ state });
  const updateProps = { appVersion: "1.0" };
  const updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  await reloadUpdateManagerData();
  await reInitUpdateService();

  const readyUpdate = await gUpdateManager.getReadyUpdate();
  if (useValidMar) {
    Assert.equal(readStatusFile(), state, "Status should not have changed");
    Assert.ok(!!readyUpdate, "Should have a ready update");
    Assert.ok(marFile.exists(), "MAR file should still exist");
  } else {
    const statusFile = getUpdateDirFile(FILE_UPDATE_STATUS);
    Assert.ok(!statusFile.exists(), "Status file should have been cleaned up");
    Assert.ok(!readyUpdate, "Should not have a ready update");
    Assert.ok(!marFile.exists(), "MAR file should have been cleaned up");
  }
}

add_task(async function initWithPendingUpdate() {
  setupTestCommon();

  const relevantStates = [
    STATE_PENDING,
    STATE_PENDING_SVC,
    STATE_APPLIED,
    STATE_APPLIED_SVC,
  ];

  await parameterizedTest(testInitWithPendingUpdate, {
    state: relevantStates,
    useValidMar: [true, false],
  });

  await doTestFinish();
});
