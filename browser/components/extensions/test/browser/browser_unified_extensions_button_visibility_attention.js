/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

let TAB_WITH_ATTENTION, TAB_WITHOUT_ATTENTION;

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.unifiedExtensions.button.always_visible", false],
      ["extensions.originControls.grantByDefault", false],
    ],
  });

  let extension;
  // ExtensionTestUtils.loadExtension registers a cleanup handler that fails
  // if the extension is still installed, so we need to register the cleanup
  // handler first, to install it before loadExtension.
  registerCleanupFunction(() => extension.unload());
  extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      manifest_version: 3,
      content_scripts: [
        {
          matches: ["https://example.com/?attention_test"],
          js: ["script.js"],
        },
      ],
    },
    files: { "script.js": "// Not relevant" },
  });
  await extension.startup();
  TAB_WITH_ATTENTION = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/?attention_test"
  );
  TAB_WITHOUT_ATTENTION = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(TAB_WITHOUT_ATTENTION);
    BrowserTestUtils.removeTab(TAB_WITH_ATTENTION);
  });
});

add_task(async function test_button_shown_by_attention() {
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonHidden();
  resetExtensionsButtonTelemetry();

  info("Switch to tab demanding attention; button should show");
  gBrowser.selectedTab = TAB_WITH_ATTENTION;
  ok(gUnifiedExtensions.button.hasAttribute("attention"), "Button attention");
  assertExtensionsButtonVisible();
  assertExtensionsButtonTelemetry({ attention_permission_denied: 1 });

  info("Switch to tab without attention; button should hide");
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonHidden();
  // Counter still the same as before.
  assertExtensionsButtonTelemetry({ attention_permission_denied: 1 });
});

add_task(async function test_button_ignore_attention_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.ignore_attention", true]],
  });
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonHidden();
  resetExtensionsButtonTelemetry();

  info("Switch to tab demanding attention; button should be hidden by pref");
  gBrowser.selectedTab = TAB_WITH_ATTENTION;
  ok(gUnifiedExtensions.button.hasAttribute("attention"), "Button attention");
  assertExtensionsButtonHidden();

  info("Switch to tab without attention; button should stay hidden");
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonHidden();
  assertExtensionsButtonTelemetry({ attention_permission_denied: 0 });
  await SpecialPowers.popPrefEnv();
});

// This test verifies that the Extensions Button stays visible with the shown
// attention dot, even while toggling the visibility of the menu item.
// This complements the test_menu_items_on_hidden_button test in
// browser_unified_extensions_button_visibility.js. In that other test, the
// button is temporarily shown and closed upon opening the context menu; in
// this test, the button continues to be visible because of the attention dot.
add_task(async function test_contextmenu_on_button_with_attention() {
  Services.fog.testResetFOG();
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  gBrowser.selectedTab = TAB_WITH_ATTENTION;
  assertExtensionsButtonVisible();

  {
    info("Open context menu on temporarily shown button, to toggle pref");
    const contextMenu = await openChromeContextMenu(
      "toolbar-context-menu",
      "#unified-extensions-button"
    );
    const removeFromToolbar = contextMenu.querySelector(
      ".customize-context-removeFromToolbar"
    );
    is(removeFromToolbar.hidden, true, "removeFromToolbar is hidden");
    const item = contextMenu.querySelector(
      "#toolbar-context-always-show-extensions-button"
    );
    is(item.hidden, false, "'Always Show in Toolbar' menu item shown");
    ok(!item.getAttribute("checked"), "Should be unchecked by pref in setup");
    info("Clicking context menu to toggle pref (to always show button)");
    await closeChromeContextMenu(contextMenu.id, item);
    assertExtensionsButtonVisible();
  }

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_customizing: "false",
        is_extensions_panel_empty: "false",
        is_temporarily_shown: "true",
        should_hide: "false",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after showing"
  );
  Services.fog.testResetFOG();

  {
    info("Open context menu on 'always shown' button, to toggle pref");
    const contextMenu = await openChromeContextMenu(
      "toolbar-context-menu",
      "#unified-extensions-button"
    );
    const removeFromToolbar = contextMenu.querySelector(
      ".customize-context-removeFromToolbar"
    );
    is(removeFromToolbar.hidden, false, "removeFromToolbar is shown");
    ok(
      !removeFromToolbar.hasAttribute("disabled"),
      "removeFromToolbar is enabled (because button was set to always visible)"
    );
    const item = contextMenu.querySelector(
      "#toolbar-context-always-show-extensions-button"
    );
    is(item.hidden, true, "'Always Show in Toolbar' menu item is hidden");
    info("Clicking context menu to toggle pref (to hide button if possible)");
    await closeChromeContextMenu(contextMenu.id, removeFromToolbar);
    info("Button should still be temporarily visible due to attention dot");
    assertExtensionsButtonVisible();
  }

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_customizing: "false",
        is_extensions_panel_empty: "false",
        is_temporarily_shown: "true",
        should_hide: "true",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after hiding"
  );

  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonHidden();
});

add_task(async function test_button_shown_by_attention_from_blocklist() {
  // Sanity check: Starting without attention.
  assertExtensionsButtonHidden();
  resetExtensionsButtonTelemetry();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: "block-me@please" } },
    },
  });
  await extension.startup();
  const addon = await AddonManager.getAddonByID(extension.id);

  const cleanupBlocklist = await loadBlocklistRawData({ blocked: [addon] });
  assertExtensionsButtonVisible();
  assertExtensionsButtonTelemetry({ attention_blocklist: 1 });

  gBrowser.selectedTab = TAB_WITH_ATTENTION;
  gBrowser.selectedTab = TAB_WITHOUT_ATTENTION;
  assertExtensionsButtonVisible();
  // When the button is already visible, adding another (attention) reason for
  // showing it does not trigger additional telemetry.
  assertExtensionsButtonTelemetry({ attention_blocklist: 1 });

  await cleanupBlocklist();
  assertExtensionsButtonHidden();
  // No change after transitioning from blocked to unblocked.
  assertExtensionsButtonTelemetry({ attention_blocklist: 1 });

  await extension.unload();
});
