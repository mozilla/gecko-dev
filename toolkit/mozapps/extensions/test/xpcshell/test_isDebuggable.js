/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var ADDONS = [
  "test_bootstrap2_1", // this test is restartless and therefore should be debuggable
  "test_bootstrap1_4", // an opposite of the above
];

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  startupManager();
  AddonManager.checkCompatibility = false;

  installAllFiles([do_get_addon(a) for each (a in ADDONS)], function() {
    restartManager();

    AddonManager.getAddonsByIDs(["bootstrap1@tests.mozilla.org",
                                 "bootstrap2@tests.mozilla.org"],
                               function([a1, a2]) {
      do_check_eq(a1.isDebuggable, false);
      do_check_eq(a2.isDebuggable, true);
      do_test_finished();
    });
  }, true);
}
