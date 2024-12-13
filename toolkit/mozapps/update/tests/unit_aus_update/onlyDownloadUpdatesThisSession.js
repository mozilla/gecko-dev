/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function setup() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");
}
setup();

add_task(async function onlyDownloadUpdatesThisSession() {
  gAUS.onlyDownloadUpdatesThisSession = true;

  await downloadUpdate({ expectDownloadRestriction: true });

  Assert.ok(
    !(await gUpdateManager.getReadyUpdate()),
    "There should not be a ready update. The update should still be downloading"
  );
  const downloadingUpdate = await gUpdateManager.getDownloadingUpdate();
  Assert.ok(!!downloadingUpdate, "A downloading update should exist");
  Assert.equal(
    downloadingUpdate.state,
    STATE_DOWNLOADING,
    "The downloading update should still be in the downloading state"
  );
});

add_task(async function finish() {
  stop_httpserver(doTestFinish);
});
