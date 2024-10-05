/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test whether old OS versions are not matched when the blocklist contains
// only new OS versions.
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
  gfxInfo.spoofDriverVersion("8.52.322.2201");
  gfxInfo.spoofVendorID("0xabcd");
  gfxInfo.spoofDeviceID("0x1234");

  // Spoof the version of the OS appropriately to test the test file.
  switch (Services.appinfo.OS) {
    case "WINNT":
      // Windows 7
      gfxInfo.spoofOSVersion(0x60001);
      break;
    case "Linux":
      // We don't have any OS versions on Linux, just "Linux".
      do_test_finished();
      return;
    case "Darwin":
      // Lion
      gfxInfo.spoofOSVersion(0xa0800);
      break;
    case "Android":
      // On Android, the driver version is used as the OS version (because
      // there's so many of them).
      do_test_finished();
      return;
  }

  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  function checkBlocklist() {
    if (Services.appinfo.OS == "WINNT") {
      var status = gfxInfo.getFeatureStatusStr("DIRECT2D");
      Assert.equal(status, "STATUS_OK");
    } else if (Services.appinfo.OS == "Darwin") {
      status = gfxInfo.getFeatureStatusStr("OPENGL_LAYERS");
      Assert.equal(status, "STATUS_OK");
    }

    do_test_finished();
  }

  Services.obs.addObserver(function () {
    // If we wait until after we go through the event loop, gfxInfo is sure to
    // have processed the gfxItems event.
    executeSoon(checkBlocklist);
  }, "blocklist-data-gfxItems");

  mockGfxBlocklistItemsFromDisk("../data/test_gfxBlocklist_OSVersion.json");
}
