/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

add_setup(async () => {
  // Simulate that the user hid the Extensions button by default.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.unifiedExtensions.button.always_visible", false]],
  });
});

// Test that the button can be shown via browserAction.openPopup() API.
add_task(async function test_show_via_browserAction_openPopup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.openPopupWithoutUserGesture.enabled", true]],
  });
  resetExtensionsButtonTelemetry();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_action: { default_popup: "popup.html" },
    },
    files: { "popup.html": "Dummy popup panel content here" },
    background() {
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("open_action_popup_panel", msg, "Expected msg");
        browser.browserAction.openPopup();
      });
    },
  });
  await extension.startup();

  // Sanity check: button hidden before we do something that should show it.
  assertExtensionsButtonHidden(win);

  info("Testing that browserAction.openPopup() opens panel + shows button");
  const popupShownPromise = awaitExtensionPanel(extension, win);
  extension.sendMessage("open_action_popup_panel");
  const popupBrowser = await popupShownPromise;
  assertExtensionsButtonVisible(win);
  assertExtensionsButtonTelemetry({ extension_browser_action_popup: 1 });

  info("Testing that the button hides when the popup panel is closed");
  let hiddenPromise = promisePopupHidden(getPanelForNode(popupBrowser));
  await closeBrowserAction(extension, win);
  await hiddenPromise;
  assertExtensionsButtonHidden(win);

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);

  // Same counter as before, nothing else whilst closing the action popup.
  assertExtensionsButtonTelemetry({ extension_browser_action_popup: 1 });

  await SpecialPowers.popPrefEnv();
});

// Test that the button can be shown via the _execute_browser_action command in
// the commands API, triggered via a user shortcut without any other extension
// API calls. Notably, the anount of processing (and time) between triggering a
// user input (keyboard shortcut) and the opening of the popup is minimal.
add_task(async function test_show_via_commands_execute_browser_action() {
  resetExtensionsButtonTelemetry();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      browser_action: { default_popup: "popup.html" },
      commands: {
        _execute_browser_action: {
          suggested_key: {
            default: "Ctrl+Shift+Comma",
            mac: "MacCtrl+Shift+Comma",
          },
        },
      },
    },
    files: { "popup.html": "Dummy popup panel content here" },
  });
  await extension.startup();

  // Sanity check: button hidden before we do something that should show it.
  assertExtensionsButtonHidden(win);

  info("Testing that _execute_browser_action opens panel + shows button");
  const popupShownPromise = awaitExtensionPanel(extension, win);
  EventUtils.synthesizeKey("VK_COMMA", { ctrlKey: true, shiftKey: true }, win);
  const popupBrowser = await popupShownPromise;
  assertExtensionsButtonVisible(win);
  assertExtensionsButtonTelemetry({ extension_browser_action_popup: 1 });

  info("Testing that the button hides when the popup panel is closed");
  let hiddenPromise = promisePopupHidden(getPanelForNode(popupBrowser));
  await closeBrowserAction(extension, win);
  await hiddenPromise;
  assertExtensionsButtonHidden(win);

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);

  // Same counter as before, nothing else whilst closing the action popup.
  assertExtensionsButtonTelemetry({ extension_browser_action_popup: 1 });
});
