/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is the script that runs in the child xpcshell process when using
// TestUpdateMutexCrossProcess, like we do in
// unit_aus_update/canCheckForAndCanApplyUpdates.js.
// The main thing this script does is try to acquire update mutex in the same
// fake environment in which the parent runs, which we must set up.
// This requires that our command line defines of the relevant helper functions
// from xpcshellUtilsAUS.js, which are:
/* global registerCustomDirProvider */
/* global resetSyncManagerLock */
/* global EXIT_CODE */

print("child process is running");

registerCustomDirProvider();
resetSyncManagerLock();

var updateMutex = Cc["@mozilla.org/updates/update-mutex;1"].createInstance(
  Ci.nsIUpdateMutex
);
if (!updateMutex.tryLock()) {
  quit(EXIT_CODE.FAILED_TO_ACQUIRE_UPDATE_MUTEX);
}

// Wait for 60 seconds, then exit. We expect that the parent will kill us first.
print(
  "child process should now have the update mutex; will exit in 60 seconds"
);
simulateNoScriptActivity(60);
print("child process exiting now");

updateMutex.unlock();
