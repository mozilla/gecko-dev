/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is the script that runs in the child xpcshell process for the test
// unit_aus_update/updateSyncManager.js.
// The main thing this script does is override the child's directory service
// so that it ends up with the same fake binary path that the parent test runner
// has opened its update lock with.
// This requires that our command line defines of the relevant helper functions
// from xpcshellUtilsAUS.js, which are:
/* global registerCustomDirProvider */
/* global resetSyncManagerLock */

print("child process is running");

registerCustomDirProvider();
resetSyncManagerLock();

// Wait a few seconds for the parent to do what it needs to do, then exit.
print("child process should now have the lock; will exit in 5 seconds");
simulateNoScriptActivity(5);
print("child process exiting now");
