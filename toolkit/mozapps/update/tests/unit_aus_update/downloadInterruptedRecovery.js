/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();
  debugDump("testing mar mar download interrupted recovery");
  // This test assumes speculative connections enabled.
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  });
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);

  await downloadUpdate({
    incrementalDownloadErrorType: 0,
    expectedCheckResult: { updateCount: 1 },
  });

  stop_httpserver(doTestFinish);
}
