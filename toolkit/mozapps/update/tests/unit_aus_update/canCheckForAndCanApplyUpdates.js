/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();

  // Verify write access to the custom app dir
  debugDump("testing write access to the application directory");
  let testFile = getCurrentProcessDir();
  testFile.append("update_write_access_test");
  testFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
  testFile.remove(false);
  Assert.ok(!testFile.exists(), MSG_SHOULD_NOT_EXIST);

  // Acquire the update mutex to prevent the current instance from being
  // able to check for or apply updates -- and check that it can't.
  if (AppConstants.platform == "win") {
    let updateMutex = Cc["@mozilla.org/updates/update-mutex;1"].createInstance(
      Ci.nsIUpdateMutex
    );

    debugDump("attempting to acquire the update mutex");
    Assert.ok(
      updateMutex.tryLock(),
      "should be able to acquire the update mutex"
    );

    try {
      // Check that available updates cannot be checked for when the update
      // mutex for this installation path is acquired.
      Assert.ok(
        !gAUS.canCheckForUpdates,
        "should not be able to check for updates when the update mutex is acquired by another instance"
      );

      // Check if updates cannot be applied when the update mutex for this
      // installation path is acquired.
      Assert.ok(
        !gAUS.canApplyUpdates,
        "should not be able to apply updates when the update mutex is acquired by another instance"
      );
    } finally {
      debugDump("releasing the update mutex");
      updateMutex.unlock();
    }
  }

  // Check that available updates can be checked for
  Assert.ok(gAUS.canCheckForUpdates, "should be able to check for updates");
  // Check that updates can be applied
  Assert.ok(gAUS.canApplyUpdates, "should be able to apply updates");

  // Attempt to acquire the update mutex(es) now that the current instance has
  // acquired it.
  if (AppConstants.platform == "win") {
    let updateMutex = Cc["@mozilla.org/updates/update-mutex;1"].createInstance(
      Ci.nsIUpdateMutex
    );

    debugDump("attempting to acquire the update mutex");
    let isAcquired = updateMutex.tryLock();
    if (isAcquired) {
      updateMutex.unlock();
    }

    Assert.ok(
      !isAcquired,
      "should not be able to acquire the update mutex when the current instance has already acquired it"
    );
  }

  await doTestFinish();
}
