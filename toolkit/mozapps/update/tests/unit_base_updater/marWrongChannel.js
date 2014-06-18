/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Test product/channel MAR security check */

function run_test() {
  if (!IS_MAR_CHECKS_ENABLED) {
    return;
  }

  setupTestCommon();
  // We don't actually care if the MAR has any data, we only care about the
  // application return code and update.status result.
  gTestFiles = gTestFilesCommon;
  gTestDirs = [];
  setupUpdaterTest(FILE_WRONG_CHANNEL_MAR, false, false);

  createUpdaterINI();

  // Apply the MAR
  // Note that if execv is used, the updater process will turn into the
  // callback process, so its return code will be that of the callback
  // app.
  runUpdate((USE_EXECV ? 0 : 1), STATE_FAILED_CHANNEL_MISMATCH_ERROR);
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateApplied() {
  if (IS_MACOSX || IS_WIN) {
    // Check that the post update process was not launched.
    do_check_false(getPostUpdateFile(".running").exists());
  }

  checkFilesAfterUpdateSuccess();
  doTestFinish();
}
