/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether a machine which is lower than the greater-than-or-equal
// blocklist entry is allowed.
// Uses test_gfxBlocklist.json

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
  switch (Services.appinfo.OS) {
    case "WINNT":
      gfxInfo.spoofVendorID("0xabab");
      gfxInfo.spoofDeviceID("0x1234");
      gfxInfo.spoofDriverVersion("8.52.322.2201");
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      // We don't support driver versions on Linux.
      do_test_finished();
      return;
    case "Darwin":
      // We don't support driver versions on Darwin.
      do_test_finished();
      return;
    case "Android":
      gfxInfo.spoofVendorID("abab");
      gfxInfo.spoofDeviceID("ghjk");
      gfxInfo.spoofDriverVersion("6");
      break;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  function checkBlocklist() {
    var status = gfxInfo.getFeatureStatusStr("DIRECT2D");
    Assert.equal(status, "STATUS_OK");

    // Make sure unrelated features aren't affected
    status = gfxInfo.getFeatureStatusStr("DIRECT3D_9_LAYERS");
    Assert.equal(status, "STATUS_OK");

    do_test_finished();
  }

  Services.obs.addObserver(function () {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlocklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlocklist.json");
}
