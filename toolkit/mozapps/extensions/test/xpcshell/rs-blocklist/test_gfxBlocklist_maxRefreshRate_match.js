/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether new OS versions are matched properly.
// Uses test_gfxBlocklist_OSVersion.json

// Performs the initial setup
async function run_test() {
  var gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);

  // We can't do anything if we can't spoof the stuff we need.
  if (!(gfxInfo instanceof Ci.nsIGfxInfoDebug)) {
    do_test_finished();
    return;
  }

  gfxInfo.QueryInterface(Ci.nsIGfxInfoDebug);

  // Set the vendor/device ID, etc, to match the test file.
  gfxInfo.spoofDriverVersion("58.52.322.2201");
  gfxInfo.spoofVendorID("0xabcd");
  gfxInfo.spoofDeviceID("0x1234");

  switch (Services.appinfo.OS) {
    case "WINNT":
      gfxInfo.spoofMonitorInfo(3, 60, 90);
      break;
    case "Linux":
      gfxInfo.spoofMonitorInfo(2, 20, 40);
      break;
    case "Darwin":
      gfxInfo.spoofMonitorInfo(3, 29, 30);
      break;
    case "Android":
      gfxInfo.spoofMonitorInfo(1, 30, 51);
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  function checkBlocklist() {
    var status = gfxInfo.getFeatureStatusStr("WEBRENDER");
    Assert.equal(status, "BLOCKED_DEVICE");
    status = gfxInfo.getFeatureStatusStr("WEBRENDER_PARTIAL_PRESENT");
    Assert.equal(status, "STATUS_OK");
    do_test_finished();
  }

  Services.obs.addObserver(function () {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlocklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk(
    "../data/test_gfxBlocklist_maxRefreshRate.json"
  );
}
