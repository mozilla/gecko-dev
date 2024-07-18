/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Verify that app post update process is launched with minimal/empty
 * environment after applying an update. */

async function run_test() {
  Services.env.set("MOZ_TEST_POST_UPDATE_VAR", "1");

  if (!setupTestCommon()) {
    return;
  }
  gTestFiles = gTestFilesCompleteSuccess;
  gTestDirs = gTestDirsCompleteSuccess;
  preventDistributionFiles();

  // The post update process invocation is `TestAUSHelper post-update-environment`.
  await setupUpdaterTest(FILE_COMPLETE_MAR, true, "", true, {
    asyncExeArg: "post-update-environment",
  });

  runUpdate(STATE_SUCCEEDED, false, 0, true);

  // This means the environment variable *is not* found.  If it is
  // found, there'll be quotes and its (possibly empty) string value.
  await checkPostUpdateAppLog({
    expectedContents: "MOZ_TEST_POST_UPDATE_VAR=\n",
  });

  checkAppBundleModTime();
  await testPostUpdateProcessing();
  checkPostUpdateRunningFile(true);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_COMPLETE_SUCCESS, false, false, true);
  await waitForUpdateXMLFiles();
  await checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);
  checkCallbackLog();
}
