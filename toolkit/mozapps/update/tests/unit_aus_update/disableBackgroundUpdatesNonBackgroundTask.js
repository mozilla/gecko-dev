/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

/**
 * This test verifies that when Balrog advertises that an update should not
 * be downloaded in the background, but we are not running in the background,
 * the advertisement does not have any effect.
 */

function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");
}
setup();

add_task(async function disableBackgroundUpdatesBackgroundTask() {
  await downloadUpdate({ updateProps: { disableBackgroundUpdates: "true" } });
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
