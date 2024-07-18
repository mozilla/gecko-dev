"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

// browser-unified-extensions.js already defines OriginControls, so we don't
// need this import:
// const { OriginControls } = ChromeUtils.importESModule(
//   "resource://gre/modules/ExtensionPermissions.sys.mjs"
// );

AddonTestUtils.initMochitest(this);

// Regression test for bug 1905392: When OriginControls.getState is called
// while an extension is initializing, it should return a meaningful result.
// We simulate that here by temporarily clearing extension.tabManager.
add_task(async function getState_for_partially_initialized_extension() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  const id = "dummy@middle";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id } },
      permissions: ["*://*/*"],
    },
  });
  await extension.startup();
  let policy = WebExtensionPolicy.getByID(id);

  // tabManager is only set when "startup" has fired. Simulate the absence of
  // "tabManager" (regression test for bug 1905392).
  let tabManager = policy.extension.tabManager;
  policy.extension.tabManager = undefined;

  Assert.deepEqual(
    OriginControls.getState(policy, tab),
    { noAccess: true },
    "getState should return noAccess for a (simulated) uninitialized extension"
  );

  policy.extension.tabManager = tabManager;

  Assert.deepEqual(
    OriginControls.getState(policy, tab),
    { allDomains: true, hasAccess: true },
    "getState should return allDomains + hasAccess for extension with *://*/*"
  );

  await extension.unload();

  // We don't care about the exact result, as long as it does not throw.
  Assert.deepEqual(
    OriginControls.getState(policy, tab),
    { allDomains: true, hasAccess: true },
    "getState should not throw upon encountering an unloaded extension"
  );

  BrowserTestUtils.removeTab(tab);
});
