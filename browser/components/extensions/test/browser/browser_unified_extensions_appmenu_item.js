/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

function menuItemThatOpensAboutAddons() {
  return PanelUI.panel.querySelector("#appMenu-extensions-themes-button");
}
function menuItemThatOpensExtensionsPanel() {
  return PanelUI.panel.querySelector("#appMenu-unified-extensions-button");
}

add_task(async function test_appmenu_when_button_is_always_shown() {
  await gCUITestUtils.openMainMenu();

  ok(
    BrowserTestUtils.isVisible(menuItemThatOpensAboutAddons()),
    "'Extensions and themes' menu item is visible by default"
  );

  ok(
    BrowserTestUtils.isHidden(menuItemThatOpensExtensionsPanel()),
    "'Extensions' menu item is hidden by default"
  );

  await gCUITestUtils.hideMainMenu();
});

add_task(async function test_appmenu_when_button_is_hidden() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
  await gCUITestUtils.openMainMenu();

  ok(
    BrowserTestUtils.isHidden(menuItemThatOpensAboutAddons()),
    "'Extensions and themes' menu item is hidden"
  );

  ok(
    BrowserTestUtils.isVisible(menuItemThatOpensExtensionsPanel()),
    "'Extensions' menu item is shown"
  );

  await gCUITestUtils.hideMainMenu();
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_appmenu_extensions_opens_panel() {
  Services.fog.testResetFOG();
  resetExtensionsButtonTelemetry();
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
  await gCUITestUtils.openMainMenu();

  assertExtensionsButtonHidden();
  menuItemThatOpensExtensionsPanel().click();
  is(PanelUI.panel.state, "closed", "Menu closed after clicking Extensions");
  // assertExtensionsButtonVisible(); cannot be checked because button showing
  // is async. We will check its visibility later, before closing the panel.

  Assert.deepEqual(
    Glean.extensionsButton.openViaAppMenu.testGetValue().map(e => e.extra),
    [
      {
        is_extensions_panel_empty: "false",
        is_extensions_button_visible: "false",
      },
    ],
    "extensions_button.open_via_app_menu telemetry on menu click"
  );

  const listView = getListView();
  await BrowserTestUtils.waitForEvent(listView, "ViewShown");
  ok(PanelView.forNode(listView).active, "Extensions panel is shown");

  // Just check that it is visible. Verification of the 'Manage Extensions'
  // button is covered by test_panel_has_a_manage_extensions_button.
  ok(
    BrowserTestUtils.isVisible(
      listView.querySelector("#unified-extensions-manage-extensions")
    ),
    "'Manage Extensions' option is visible"
  );

  // Sanity check to show that we indeed had extensions, showing that this test
  // case differs from test_appmenu_extensions_opens_when_no_extensions.
  ok(gUnifiedExtensions.hasExtensionsInPanel(), "Sanity check: has extensions");

  assertExtensionsButtonVisible();
  assertExtensionsButtonTelemetry({ extensions_panel_showing: 1 });
  await closeExtensionsPanel();
  assertExtensionsButtonHidden();

  // No more counters besides the one that we saw before in this test.
  assertExtensionsButtonTelemetry({ extensions_panel_showing: 1 });

  await SpecialPowers.popPrefEnv();
});

// When the Extensions Button is visible, it opens about:addons upon click.
// Do the same when the Extensions app menu item is clicked.
// This behavior differs from test_appmenu_extensions_opens_panel.
add_task(async function test_appmenu_extensions_opens_when_no_extensions() {
  // The test harness registers regular extensions so we need to mock the
  // `getActivePolicies` extension to simulate zero extensions installed.
  const origGetActivePolicies = gUnifiedExtensions.getActivePolicies;
  gUnifiedExtensions.getActivePolicies = () => [];

  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
  await gCUITestUtils.openMainMenu();

  const listener = () => {
    ok(false, "Extensions Panel should not be shown");
  };
  gUnifiedExtensions.panel.addEventListener("popupshowing", listener);

  // When the list of extensions is empty, the menu opens about:addons.
  // Open a new non-newtab page so that about:addons will be opened in a new
  // tab (instead of reusing the "current" new tab).
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:robots" },
    async () => {
      let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:addons");

      assertExtensionsButtonHidden();
      menuItemThatOpensExtensionsPanel().click();
      assertExtensionsButtonHidden();
      info("Verifying that about:addons is opened");
      BrowserTestUtils.removeTab(await tabPromise);
    }
  );

  assertExtensionsButtonHidden();
  gUnifiedExtensions.panel.removeEventListener("popupshowing", listener);

  Assert.deepEqual(
    Glean.extensionsButton.openViaAppMenu.testGetValue().map(e => e.extra),
    [
      {
        is_extensions_panel_empty: "true",
        is_extensions_button_visible: "false",
      },
    ],
    "extensions_button.open_via_app_menu telemetry on menu click"
  );

  await SpecialPowers.popPrefEnv();

  gUnifiedExtensions.getActivePolicies = origGetActivePolicies;
});

// A hidden Extensions Button can temporarily be shown when an attention dot is
// shown.
add_task(async function test_appmenu_extensions_with_attention_dot() {
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      // Attention dot is shown when MV3 permissions are not granted, so
      // prevent auto-granting of permissions on install.
      ["extensions.originControls.grantByDefault", false],
      ["extensions.unifiedExtensions.button.always_visible", false],
    ],
  });

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_specific_settings: { gecko: { id: "test@attention-dot" } },
      manifest_version: 3,
      host_permissions: ["https://example.com/*"],
    },
  });
  await extension.startup();

  // Trigger attention dot and open the appmenu item. The attention dot has
  // more tests in browser_unified_extensions_button_visibility_attention.js.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "https://example.com/?test-attention-dot" },
    async () => {
      assertExtensionsButtonVisible();
      await gCUITestUtils.openMainMenu();
      menuItemThatOpensExtensionsPanel().click();
      const listView = getListView();
      await BrowserTestUtils.waitForEvent(listView, "ViewShown");
      ok(PanelView.forNode(listView).active, "Extensions panel is shown");
      await closeExtensionsPanel();
      assertExtensionsButtonVisible();
    }
  );

  Assert.deepEqual(
    Glean.extensionsButton.openViaAppMenu.testGetValue().map(e => e.extra),
    [
      {
        is_extensions_panel_empty: "false",
        is_extensions_button_visible: "true",
      },
    ],
    "extensions_button.open_via_app_menu telemetry on menu click"
  );

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
