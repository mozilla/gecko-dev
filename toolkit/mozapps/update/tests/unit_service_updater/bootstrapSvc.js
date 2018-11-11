/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Bootstrap the tests using the service by installing our own version of the service */

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  // We don't actually care if the MAR has any data, we only care about the
  // application return code and update.status result.
  gTestFiles = gTestFilesCommon;
  gTestDirs = [];
  setupUpdaterTest(FILE_COMPLETE_MAR, null);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runUpdate(STATE_SUCCEEDED, false, 0, true);
}

/**
 * Called after the call to runUpdateUsingService finishes.
 */
function runUpdateFinished() {
  checkFilesAfterUpdateSuccess(getApplyDirFile, false, false);

  // We need to check the service log even though this is a bootstrap
  // because the app bin could be in use by this test by the time the next
  // test runs.
  checkCallbackLog();
}
