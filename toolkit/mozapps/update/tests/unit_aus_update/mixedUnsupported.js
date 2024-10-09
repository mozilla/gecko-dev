/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This tests that an older update is chosen over a more recent but unsupported
 * update.
 */

async function run_test() {
  setupTestCommon();
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  setUpdateChannel("test_channel");

  let patchProps = {
    type: "complete",
    url: "http://complete/",
    size: "9856459",
  };
  let patches = getRemotePatchString(patchProps);
  patchProps = { type: "partial", url: "http://partial/", size: "1316138" };
  patches += getRemotePatchString(patchProps);

  let oldAppVersion = "900000.0";
  let newAppVersion = "999999.0";
  let update1 = getRemoteUpdateString(
    { appVersion: newAppVersion, unsupported: true },
    patches
  );
  let update2 = getRemoteUpdateString({ appVersion: oldAppVersion }, patches);
  gResponseBody = getRemoteUpdatesXMLString(update1 + update2);

  let checkResult = await waitForUpdateCheck(true, { updateCount: 2 });
  let bestUpdate = await gAUS.selectUpdate(checkResult.updates);
  bestUpdate.QueryInterface(Ci.nsIWritablePropertyBag);
  Assert.equal(
    bestUpdate.unsupported,
    false,
    "The unsupported update has been discarded."
  );
  Assert.equal(
    bestUpdate.appVersion,
    oldAppVersion,
    "Expected the older version to be chosen over the more recent but unsupported."
  );

  stop_httpserver(doTestFinish);
}
