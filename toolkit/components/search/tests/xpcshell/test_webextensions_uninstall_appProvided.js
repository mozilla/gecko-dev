/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that application-provided WebExtensions are uninstalled during idle
 * background checks and ensures manually installed add-on engines are unaffected.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

add_setup(async function () {
  await SearchTestUtils.initXPCShellAddonManager();
  await Services.search.init();
});

add_task(async function test_uninstall_appProvided_extension() {
  let path = `resource://search-extensions/ddg/`;
  await AddonManager.installBuiltinAddon(path);

  let policy = WebExtensionPolicy.getByID("ddg@search.mozilla.org");
  // On startup the extension may have not finished parsing the
  // manifest, wait for that here.
  await policy.readyPromise;

  Assert.ok(policy, "Should have installed the app provided extension");

  info("Uninstall app provided extension on idle");
  await Services.search.runBackgroundChecks();

  policy = WebExtensionPolicy.getByID("ddg@search.mozilla.org");
  Assert.ok(!policy, "Should have uninstalled the app provided extension");
});

add_task(async function test_installed_addon_engine_remains() {
  await SearchTestUtils.installSearchExtension({
    name: "bacon",
  });
  let engine1 = await Services.search.getEngineByName("bacon");

  Assert.ok(engine1, "Should have Add-on engine installed.");
  await Services.search.runBackgroundChecks();

  engine1 = await Services.search.getEngineByName("bacon");

  Assert.ok(engine1, "Should have Add-on engine remain installed.");
});
