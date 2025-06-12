/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

const PREF_ALWAYS_VISIBLE =
  "extensions.unifiedExtensions.button.always_visible";

function showButtonWithPref() {
  info(`showButtonWithPref: Setting ${PREF_ALWAYS_VISIBLE} to true`);
  Services.prefs.setBoolPref(PREF_ALWAYS_VISIBLE, true);
}

function hideButtonWithPref() {
  info(`hideButtonWithPref: Setting ${PREF_ALWAYS_VISIBLE} to false`);
  Services.prefs.setBoolPref(PREF_ALWAYS_VISIBLE, false);
}

function resetButtonVisibilityToDefault() {
  Services.prefs.clearUserPref(PREF_ALWAYS_VISIBLE);
}

function assertTelemetryValueMatchesAlwaysVisiblePref() {
  is(
    Glean.extensionsButton.prefersHiddenButton.testGetValue(),
    !Services.prefs.getBoolPref(PREF_ALWAYS_VISIBLE),
    "extensions_button.prefers_hidden_button is inverse of pref value"
  );
}

async function checkAndDismissPostHideNotification(win) {
  // After hiding the extensions button, a notification is displayed for 3
  // seconds, notifying the user of "Move to menu". Check that it is shown and
  // dismiss the notification.
  info("Verifying that the 'Moved to menu' hint is shown");
  let hintElem = win.ConfirmationHint._panel;
  if (hintElem.state !== "open") {
    info("Waiting for hint to be shown");
    await BrowserTestUtils.waitForEvent(hintElem, "popupshown");
  }
  is(hintElem.state, "open", "Hint popup is open");
  is(hintElem.anchorNode.id, "PanelUI-menu-button", "Anchored to appmenu");
  is(
    win.ConfirmationHint._message.getAttribute("data-l10n-id"),
    "confirmation-hint-extensions-button-hidden",
    "Shown 'Moved to menu' notification"
  );
  let hiddenpromise = BrowserTestUtils.waitForEvent(hintElem, "popuphidden");
  hintElem.hidePopup();
  info("Waiting for hint to be dismissed");
  await hiddenpromise;
}

// Tests in this file repeatedly flips prefs. To avoid having to balance
// pushPrefEnv / popPrefEnv often, reset it once in the end.
registerCleanupFunction(resetButtonVisibilityToDefault);

add_task(async function test_default_button_visibility() {
  assertExtensionsButtonVisible();
  // assertTelemetryValueMatchesAlwaysVisiblePref() cannot be used because the
  // pref is unset by default.
  is(
    Glean.extensionsButton.prefersHiddenButton.testGetValue(),
    false,
    "extensions_button.prefers_hidden_button is false by default"
  );
});

add_task(async function test_hide_button_before_new_window() {
  hideButtonWithPref();
  assertTelemetryValueMatchesAlwaysVisiblePref();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  assertExtensionsButtonHidden(win);

  showButtonWithPref();
  assertTelemetryValueMatchesAlwaysVisiblePref();
  assertExtensionsButtonVisible(win);

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});

add_task(async function test_show_button_before_new_window() {
  showButtonWithPref();
  assertTelemetryValueMatchesAlwaysVisiblePref();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  assertExtensionsButtonVisible(win);

  hideButtonWithPref();
  assertTelemetryValueMatchesAlwaysVisiblePref();
  assertExtensionsButtonHidden(win);

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});

add_task(async function test_delay_hide_button_while_mouse_is_on_toolbar() {
  // Another window, to help with verifying that the delay in hiding buttons
  // only applies to the window that the user is interacting with.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  resetExtensionsButtonTelemetry();

  const navbar = win.document.getElementById("nav-bar");
  navbar.dispatchEvent(new win.CustomEvent("mouseenter"));

  // The (user) intent of the following is to hide the button, but we shall
  // override that intent temporarily while we detect the mouse as being on
  // the toolbar, to prevent the interface from shifting.
  hideButtonWithPref();

  info("Extensions button should immediately be hidden in another window");
  assertExtensionsButtonHidden(window);

  info("Extensions button should still be shown while mouse is on toolbar");
  assertExtensionsButtonVisible(win);

  navbar.dispatchEvent(new win.CustomEvent("mouseleave"));

  info("Extensions button should hide after the mouse goes off the toolbar");
  assertExtensionsButtonHidden(win);

  // Prolonging the button visibility state by mouse hovering does not count
  // in telemetry as a trigger to temporarily showing the button.
  assertExtensionsButtonTelemetry({});

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});

add_task(async function test_hide_button_via_contextmenu() {
  Services.fog.testResetFOG();
  // Open another window, just to see that removal from the toolbar in one
  // window also applies to another.
  const win = await BrowserTestUtils.openNewBrowserWindow();

  const contextMenu = await openChromeContextMenu(
    "toolbar-context-menu",
    "#unified-extensions-button",
    win
  );
  const removeFromToolbar = contextMenu.querySelector(
    ".customize-context-removeFromToolbar"
  );
  is(removeFromToolbar.hidden, false, "removeFromToolbar is visible");
  ok(!removeFromToolbar.hasAttribute("disabled"), "removeFromToolbar enabled");

  await closeChromeContextMenu(contextMenu.id, removeFromToolbar, win);

  info("Extensions button should hide after choosing 'Remove from Toolbar'");
  assertExtensionsButtonHidden(win);

  info("Extensions button should also be hidden in another window");
  assertExtensionsButtonHidden(window);

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_customizing: "false",
        is_extensions_panel_empty: "false",
        is_temporarily_shown: "false",
        should_hide: "true",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after hiding"
  );

  await checkAndDismissPostHideNotification(win);

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});

// Despite the button being hidden by pref, there are ways for the button to
// still show up. This checks whether the menu items appear as expected, and
// that the user can reveal the button again.
add_task(async function test_menu_items_on_hidden_button() {
  Services.fog.testResetFOG();

  hideButtonWithPref();

  // Simulate the extensions button being unhidden for whatever reason.
  // An example of a real-world scenario is when the user right-clicks on the
  // button while an extension popup was being displayed. Upon right-clicking,
  // the panel closes and the button is hidden.
  // NOTE: For a test case where the button continues to be shown upon opening
  // the context menu, see test_contextmenu_on_button_with_attention in
  // browser_unified_extensions_button_visibility_attention.js, and
  // test_customization_button_and_menu_item_visibility in this file.
  gUnifiedExtensions.button.hidden = false;
  const contextMenu = await openChromeContextMenu(
    "toolbar-context-menu",
    "#unified-extensions-button"
  );
  gUnifiedExtensions.button.hidden = true;
  assertExtensionsButtonHidden();

  const removeFromToolbar = contextMenu.querySelector(
    ".customize-context-removeFromToolbar"
  );
  is(removeFromToolbar.hidden, true, "removeFromToolbar is hidden");
  ok(removeFromToolbar.hasAttribute("disabled"), "removeFromToolbar disabled");

  const item = contextMenu.querySelector(
    "#toolbar-context-always-show-extensions-button"
  );
  is(item.hidden, false, "'Always Show in Toolbar' menu item shown");
  ok(!item.hasAttribute("checked"), "Should be unchecked to reflect pref");

  await closeChromeContextMenu(contextMenu.id, item);
  assertExtensionsButtonVisible();

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_extensions_panel_empty: "false",
        is_customizing: "false",
        is_temporarily_shown: "false",
        should_hide: "false",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after showing"
  );

  // After unhiding the button, the menu items should be the defaults:
  // - removeFromToolbar: from disabled to enabled.
  // - 'Always Show in Toolbar': from visible to hidden.
  info("Checking context menu after unhiding the button");
  const contextMenu2 = await openChromeContextMenu(
    "toolbar-context-menu",
    "#unified-extensions-button"
  );
  is(contextMenu, contextMenu2, "Context menu is the same");
  is(removeFromToolbar.hidden, false, "removeFromToolbar is visible");
  ok(!removeFromToolbar.hasAttribute("disabled"), "removeFromToolbar enabled");
  is(item.hidden, true, "'Always Show in Toolbar' menu item is hidden");

  await closeChromeContextMenu(contextMenu2.id);

  resetButtonVisibilityToDefault();
});

add_task(async function test_customization_option_hidden_if_not_customizing() {
  const contextMenu = await openChromeContextMenu(
    "toolbar-context-menu",
    "#unified-extensions-button"
  );
  const item = document.getElementById(
    "toolbar-context-always-show-extensions-button"
  );
  is(item.hidden, true, "Not expecting menu item to hide button");
  await closeChromeContextMenu(contextMenu.id);
});

// Tests that the "Always Show in Toolbar" checkbox is visible in the menu and
// reflects the expected state when entering/exiting customization mode.
// And that the Extensions Button is always shown while in customization mode.
add_task(async function test_customization_button_and_menu_item_visibility() {
  Services.fog.testResetFOG();
  resetExtensionsButtonTelemetry();

  const win = await BrowserTestUtils.openNewBrowserWindow();

  await openCustomizationUI(win);
  assertExtensionsButtonVisible();
  // When the button is always visible, entering customization mode should not
  // trigger telemetry.
  assertExtensionsButtonTelemetry({});
  {
    info("Toggle checkbox via context menu, from on to off");
    const contextMenu = await openChromeContextMenu(
      "toolbar-context-menu",
      "#wrapper-unified-extensions-button",
      win
    );
    const item = win.document.getElementById(
      "toolbar-context-always-show-extensions-button"
    );
    is(item.hidden, false, "Menu item should be visible");
    is(item.getAttribute("checked"), "true", "Should be checked by default");
    await closeChromeContextMenu(contextMenu.id, item, win);

    info("The button should still be visible while customizing");
    assertExtensionsButtonVisible(win);
    info("The button should be hidden in windows that are not customizing");
    assertExtensionsButtonHidden();

    await checkAndDismissPostHideNotification(win);
  }

  // Whilst in Customize Mode, the button stays visible even after toggling the
  // option to hide it, but we do not count it in telemetry because it was not
  // hidden before.
  assertExtensionsButtonTelemetry({});

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_customizing: "true",
        is_extensions_panel_empty: "false",
        is_temporarily_shown: "true",
        should_hide: "true",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after hiding"
  );
  Services.fog.testResetFOG();

  {
    info("Open context menu to verify checked state, then cancel menu");
    const contextMenu = await openChromeContextMenu(
      "toolbar-context-menu",
      "#wrapper-unified-extensions-button",
      win
    );
    const item = win.document.getElementById(
      "toolbar-context-always-show-extensions-button"
    );
    is(item.hidden, false, "Menu item should be visible");
    ok(!item.getAttribute("checked"), "Should be unchecked by earlier action");
    await closeChromeContextMenu(contextMenu.id, null, win);
  }

  await closeCustomizationUI(win);
  info("The button should be hidden after exiting customization");
  assertExtensionsButtonHidden(win);

  await openCustomizationUI(win);
  info("The button should be visible upon entering customization");
  assertExtensionsButtonVisible(win);
  assertExtensionsButtonTelemetry({ customize: 1 });
  {
    info("Toggle checkbox via context menu, from off to on");
    const contextMenu = await openChromeContextMenu(
      "toolbar-context-menu",
      "#wrapper-unified-extensions-button",
      win
    );
    const item = win.document.getElementById(
      "toolbar-context-always-show-extensions-button"
    );
    is(item.hidden, false, "Menu item should be visible");
    ok(!item.hasAttribute("checked"), "Should be unchecked");
    await closeChromeContextMenu(contextMenu.id, item, win);

    info("After checking the option, buttons should be shown in all windows");
    assertExtensionsButtonVisible(win);
    assertExtensionsButtonVisible();
  }

  Assert.deepEqual(
    Glean.extensionsButton.toggleVisibility.testGetValue().map(e => e.extra),
    [
      {
        is_extensions_panel_empty: "false",
        is_customizing: "true",
        is_temporarily_shown: "true",
        should_hide: "false",
      },
    ],
    "Expected extensions_button.toggle_visibility telemetry after showing"
  );

  await closeCustomizationUI(win);
  await BrowserTestUtils.closeWindow(win);

  // In the whole test, we should have increment the counters only once: when
  // transitioning into Customize Mode when the button was hidden.
  assertExtensionsButtonTelemetry({ customize: 1 });

  resetButtonVisibilityToDefault();
});
