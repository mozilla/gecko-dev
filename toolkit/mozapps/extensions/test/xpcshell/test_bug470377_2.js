/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

var ADDONS = [
  "test_bug470377_1",
  "test_bug470377_2",
  "test_bug470377_3",
  "test_bug470377_4",
  "test_bug470377_5",
];

Components.utils.import("resource://testing-common/httpd.js");
var server;

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  server = new HttpServer();
  server.registerDirectory("/", do_get_file("data/test_bug470377"));
  server.start(-1);

  startupManager();
  AddonManager.checkCompatibility = false;

  installAllFiles(ADDONS.map(a => do_get_addon(a)), function() {
    restartManager();

    AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                                 "bug470377_2@tests.mozilla.org",
                                 "bug470377_3@tests.mozilla.org",
                                 "bug470377_4@tests.mozilla.org",
                                 "bug470377_5@tests.mozilla.org"],
                                 function([a1, a2, a3, a4, a5]) {
      do_check_eq(a1, null);
      do_check_neq(a2, null);
      do_check_neq(a3, null);
      do_check_neq(a4, null);
      do_check_neq(a5, null);

      server.stop(do_test_finished);
    });
  }, true);
}
