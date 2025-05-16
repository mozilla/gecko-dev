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

// Tests in this file repeatedly flips prefs. To avoid having to balance
// pushPrefEnv / popPrefEnv often, reset it once in the end.
registerCleanupFunction(resetButtonVisibilityToDefault);

add_task(async function test_default_button_visibility() {
  assertExtensionsButtonVisible();
});

add_task(async function test_hide_button_before_new_window() {
  hideButtonWithPref();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  assertExtensionsButtonHidden(win);

  showButtonWithPref();
  assertExtensionsButtonVisible(win);

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});

add_task(async function test_show_button_before_new_window() {
  showButtonWithPref();
  const win = await BrowserTestUtils.openNewBrowserWindow();
  assertExtensionsButtonVisible(win);

  hideButtonWithPref();
  assertExtensionsButtonHidden(win);

  await BrowserTestUtils.closeWindow(win);
  resetButtonVisibilityToDefault();
});
