/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const STATE_IDLE = 1;

// This test ensures when an update that uses service fails, the updater
// will fall back and try again without using service, but it will not do so repeatedly.
// If the failed update did not use service initially, the updater should not fall back at all.
add_task(async function updateFailureTest() {
  setupTestCommon();
  // Simulate failed update with service.
  simulateUpdateState("pending-service", "failed:7");
  reloadUpdateManagerData(false);
  let update = await gUpdateManager.getReadyUpdate();

  Assert.ok(update, "there should be a ready update");

  Assert.equal(
    update.state,
    STATE_PENDING,
    "update state should fall back to pending"
  );

  Assert.equal(
    readStatusFile(),
    STATE_PENDING,
    "status file state should fall back to pending"
  );

  // Simulate the same update, now in STATE_PENDING, failing again.
  simulateUpdateState("pending", "failed:7");
  reloadUpdateManagerData(false);
  await testPostUpdateProcessing();

  update = await gUpdateManager.getReadyUpdate();

  Assert.ok(
    !update,
    "handleFallbackToCompleteUpdate should have cleared readyUpdate"
  );

  Assert.equal(
    gAUS.currentState,
    STATE_IDLE,
    "AUS.currentState should be idle as we expect handleFallbackToCompleteUpdate() to be called"
  );

  // If the failed update did not use service, handleFallbackToCompleteUpdate should be called
  // instead of falling back to STATE_PENDING
  simulateUpdateState("applied", "failed:7");
  reloadUpdateManagerData(false);
  await testPostUpdateProcessing();
  update = await gUpdateManager.getReadyUpdate();

  Assert.ok(
    !update,
    "handleFallbackToCompleteUpdate should have cleared readyUpdate"
  );

  Assert.equal(
    gAUS.currentState,
    STATE_IDLE,
    "AUS.currentState should be idle as we expect handleFallbackToCompleteUpdate() to be called"
  );

  executeSoon(doTestFinish);
});

function simulateUpdateState(updateState, statusFileState) {
  const XML_UPDATE = `<?xml version="1.0"?>
    <updates xmlns="http://www.mozilla.org/2005/app-update">
      <update appVersion="1.0" buildID="20080811053724" channel="default"
              displayVersion="Version 1.0" installDate="1238441400314"
              platformVersion="1.0" isCompleteUpdate="true" name="Update Test 1.0" type="minor"
              detailsURL="http://example.com/" previousAppVersion="1.0"
              serviceURL="https://example.com/" statusText="The Update was successfully installed"
              foregroundDownload="true"
              actions="showURL"
              openURL="1.0">
        <patch type="complete" URL="http://example.com/" size="775" selected="true" state="${updateState}"/>
      </update>
    </updates>`;
  writeUpdatesToXMLFile(XML_UPDATE, true);
  writeStatusFile(statusFileState);
}
