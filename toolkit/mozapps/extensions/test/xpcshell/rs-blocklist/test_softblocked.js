/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// TODO bug 1649906: strip blocklist v2-specific parts of this test.
const useMLBF = Services.prefs.getBoolPref(
  "extensions.blocklist.useMLBF",
  true
);

// Enable soft-blocks support on MLBF blocklist by default while running
// this test file.
if (useMLBF) {
  Services.prefs.setBoolPref("extensions.blocklist.softblock.enabled", true);
}

// Tests that an appDisabled add-on that becomes softBlocked remains disabled
// when becoming appEnabled
add_task(async function test_softblock() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  await promiseStartupManager();

  await promiseInstallWebExtension({
    manifest: {
      name: "Softblocked add-on",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "softblock1@tests.mozilla.org",
          strict_min_version: "2",
          strict_max_version: "3",
        },
      },
    },
  });
  let s1 = await promiseAddonByID("softblock1@tests.mozilla.org");

  // Make sure to mark it as previously enabled.
  await s1.enable();

  Assert.ok(!s1.softDisabled);
  Assert.ok(s1.appDisabled);
  Assert.ok(!s1.isActive);

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: {
          softblocked: ["softblock1@tests.mozilla.org:1.0"],
          blocked: [],
          unblocked: [],
        },
        stash_time: Date.now(),
      },
    ],
    // TODO: (Bug 1649906) remove this blocklist test data along with
    // removing Blocklist v2 implementation.
    extensions: [
      {
        guid: "softblock1@tests.mozilla.org",
        versionRange: [
          {
            severity: "1",
          },
        ],
      },
    ],
  });

  Assert.ok(s1.softDisabled);
  Assert.ok(s1.appDisabled);
  Assert.ok(!s1.isActive);

  AddonTestUtils.appInfo.platformVersion = "2";
  await promiseRestartManager("2");

  s1 = await promiseAddonByID("softblock1@tests.mozilla.org");

  Assert.ok(s1.softDisabled);
  Assert.ok(!s1.appDisabled);
  Assert.ok(!s1.isActive);
});
