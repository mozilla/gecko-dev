"use strict";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.originControls.grantByDefault", false]],
  });
});

// This tests that the attention dot is updated in another window when the
// button is pinned in one window. Regression test for bug 1967564.
//
// The single-window case is covered by another test (from bug 1802925) in
// test_attention_dot_when_pinning_extension in browser_ext_originControls.js.
add_task(async function test_attention_dot_when_pinning_in_another_window() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: "action@wants-attention" } },
      manifest_version: 3,
      action: { default_area: "navbar" },
      host_permissions: ["https://example.com/*"],
    },
  });
  await extension.startup();

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/?this-is-another-window-while-pinning"
  );
  const newWindow = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab(
    newWindow.gBrowser,
    "https://example.com/?this-is-window-where-we-pin-an-extension-button"
  );

  const extensionWidgetID = AppUiTestInternals.getBrowserActionWidgetId(
    extension.id
  );
  const contextMenu = await openChromeContextMenu(
    "toolbar-context-menu",
    `#${CSS.escape(extensionWidgetID)}`,
    newWindow
  );

  let pinToToolbar = contextMenu.querySelector(
    ".customize-context-pinToToolbar"
  );
  ok(pinToToolbar, "expected a 'Pin to Toolbar' menu item");
  ok(!pinToToolbar.hidden, "Pin to Toolbar is visible");

  ok(
    !newWindow.document.querySelector("#unified-extensions-button[attention]"),
    "Foreground window's Extensions Button has no attention dot before unpinning"
  );
  ok(
    !document.querySelector("#unified-extensions-button[attention]"),
    "Background window's Extensions Button has no attention dot before unpinning"
  );

  await closeChromeContextMenu(contextMenu.id, pinToToolbar, newWindow);

  ok(
    newWindow.document.querySelector("#unified-extensions-button[attention]"),
    "Foreground window's Extensions Button has attention dot after unpinning"
  );
  ok(
    document.querySelector("#unified-extensions-button[attention]"),
    "Background window's Extensions Button has attention dot after unpinning"
  );

  await BrowserTestUtils.closeWindow(newWindow);

  BrowserTestUtils.removeTab(tab);

  await extension.unload();

  // Work-around for bug 1885885 : When this test runs with --verify or --repeat=2
  // then the test would fail at the openChromeContextMenu() call above,
  // because the button is hidden (unpinned) by the previous test run, and this
  // state is persisted (in "browser.uiCustomization.state" pref) because the
  // button state is not cleared when an extension is uninstalled. Work around
  // this by manually clearing the state at the end of the test run.
  CustomizableUI.getTestOnlyInternalProp("gSeenWidgets").delete(
    extensionWidgetID
  );
  CustomizableUI.removeWidgetFromArea(extensionWidgetID);
});
