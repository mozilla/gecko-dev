/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Relative working directory path failure test */

/* The service cannot safely write update.status for this failure because the
 * check is done before validating the installed updater. */
const STATE_AFTER_RUNUPDATE =
  IS_SERVICE_TEST ? STATE_PENDING_SVC
                  : STATE_FAILED_INVALID_WORKING_DIR_PATH_ERROR;

function run_test() {
  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  setTestFilesAndDirsForFailure();
  setupUpdaterTest(FILE_COMPLETE_MAR, false);
}

/**
 * Called after the call to setupUpdaterTest finishes.
 */
function setupUpdaterTestFinished() {
  runUpdate(STATE_AFTER_RUNUPDATE, false, 1, true, null, null, "test", null);
}

/**
 * Called after the call to runUpdateUsingUpdater finishes.
 */
function runUpdateFinished() {
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateFailure(getApplyDirFile);
  executeSoon(waitForUpdateXMLFiles);
}

/**
 * Called after the call to waitForUpdateXMLFiles finishes.
 */
function waitForUpdateXMLFilesFinished() {
  if (IS_SERVICE_TEST) {
    checkUpdateManager(STATE_NONE, false, STATE_PENDING_SVC, 0, 1);
  } else {
    checkUpdateManager(STATE_NONE, false, STATE_FAILED,
                       INVALID_WORKING_DIR_PATH_ERROR, 1);
  }

  waitForFilesInUse();
}
