/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../../extensions/newtab/test/xpcshell/head.js */

const {
  AboutNewTabResourceMapping,
  BUILTIN_ADDON_ID,
  DISABLE_NEWTAB_AS_ADDON_PREF,
} = ChromeUtils.importESModule(
  "resource:///modules/AboutNewTabResourceMapping.sys.mjs"
);

// NOTE: this test verifies that when the browser.newtabpage.disableNewTabAsAddon
// is set to true (set on the xpcshell.toml side for this specific test file),
// then the AboutNewTabResourceMapping module (already initializated by the
// setup task added from the head.js support file) is mapping the resources
// bundled in the Desktop omni jar without going through the add-ons rootURI.

add_task(async function test_pref_sanity_check() {
  Assert.equal(
    Services.prefs.getBoolPref(DISABLE_NEWTAB_AS_ADDON_PREF, false),
    true,
    "Expected disableNewTabAsAddon pref to be true"
  );
  Assert.equal(
    AboutNewTabResourceMapping.newTabAsAddonDisabled,
    true,
    "Expected AboutNewTabResourceMapping.newTabAsAddonDisabled to be true"
  );
});

add_task(async function test_bundled_resource_mapping() {
  assertNewTabResourceMapping();
});

add_task(async function test_AboutNewTabResourceMapping() {
  Assert.equal(
    AboutNewTabResourceMapping._addonVersion,
    null,
    "Expected AboutNewTabResourceMapping _addonVersion to be null"
  );

  const resProto = Cc[
    "@mozilla.org/network/protocol;1?name=resource"
  ].getService(Ci.nsIResProtocolHandler);
  const expectedRootURISpec = `${resProto.getSubstitution("builtin-addons").spec}newtab/`;
  Assert.equal(
    AboutNewTabResourceMapping._rootURISpec,
    expectedRootURISpec,
    "Got the expected AboutNewTabResourceMapping rootURISpec"
  );

  Assert.equal(
    AboutNewTabResourceMapping._addonListener,
    null,
    "Expected no addon listener"
  );

  let policy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
  ok(policy, "Found a WebExtensionPolicy instance for the builtin addon id");
  const { id, rootURI, version } =
    AboutNewTabResourceMapping.getPreferredMapping();
  Assert.deepEqual(
    { id, rootURI: rootURI.spec, version },
    {
      id: null,
      rootURI: expectedRootURISpec,
      version: null,
    },
    "AboutNewTabResourceMapping.getPreferredMapping ignores active builtin addon"
  );
});

add_task(async function test_parentprocess_fetch() {
  let addon = await AddonManager.getAddonByID(BUILTIN_ADDON_ID);
  ok(addon, "Found builtin addon");
  Assert.equal(addon.isActive, true, "Expect add-on initially active");
  Assert.equal(
    addon.locationName,
    "app-builtin-addons",
    "Expected add-on to be in the builtin location"
  );
  await addon.disable({ allowSystemAddons: true });
  let policy = WebExtensionPolicy.getByID(BUILTIN_ADDON_ID);
  ok(
    !policy,
    "No WebExtensionPolicy instance should be found for the disabled built-in add-on"
  );

  const bundleResReq = await fetch(
    "resource://newtab/data/content/activity-stream.bundle.js"
  );
  Assert.equal(
    bundleResReq.status,
    200,
    "resource://newtab fetch should be successful"
  );

  const cssChromeReq = await fetch(
    "chrome://newtab/content/css/activity-stream.css"
  );
  Assert.equal(
    cssChromeReq.status,
    200,
    "chrome://newtab fetch should be successfull"
  );
});
