/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test verifies that the update sync manager is working correctly by
// a) making sure we're the only one that's opened it to begin with, and then
// b) starting a second copy of the same binary and making sure we can tell we
//    are no longer the only one that's opened it.

add_task(async function () {
  setupTestCommon();

  // First check that we believe we exclusively hold the lock.
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  Assert.ok(
    !syncManager.isOtherInstanceRunning(),
    "no other instance is running yet"
  );

  // Now start a second copy of this xpcshell binary so that something else
  // takes the same lock. Run the second copy two times, to show the lock is
  // usable after having been closed.
  for (let runs = 0; runs < 2; runs++) {
    // Actually invoke the process.
    runInSubprocessWithPrelude(getTestDirFile("syncManagerTestChild.js").path);

    // It will take the new xpcshell a little time to start up, but we should see
    // the effect on the lock within at most a few seconds.
    await TestUtils.waitForCondition(
      () => syncManager.isOtherInstanceRunning(),
      "waiting for child process to take the lock"
    ).catch(_e => {
      // Rather than throwing out of waitForCondition(), catch and log the failure
      // manually so that we get output that's a bit more readable.
      Assert.ok(
        syncManager.isOtherInstanceRunning(),
        "child process has the lock"
      );
    });

    // The lock should have been closed when the process exited, but we'll allow
    // a little time for the OS to clean up the handle.
    await TestUtils.waitForCondition(
      () => !syncManager.isOtherInstanceRunning(),
      "waiting for child process to release the lock"
    ).catch(_e => {
      Assert.ok(
        !syncManager.isOtherInstanceRunning(),
        "child process has released the lock"
      );
    });
  }

  await doTestFinish();
});
