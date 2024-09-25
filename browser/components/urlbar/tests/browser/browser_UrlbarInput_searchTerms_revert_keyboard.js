/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when search terms are shown
// and the user reverts the Urlbar using the revert button via keyboard events.
// It's also to ensure that keyboard navigating the toolbar doesn't break.

// The main search keyword used in tests
const SEARCH_STRING = "chocolate cake";

add_setup(async function () {
  // Ideally, we could also check tab keyboard event works without the presence
  // of the search mode switcher button, but while its disabled, the element
  // selected when Shift + Tab is pressed when the input is focused in an
  // invalid pageproxystate can be inconsistent in OSX TV tests.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.showSearchTerms.featureGate", true],
      ["browser.urlbar.scotchBonnet.enableOverride", true],
    ],
  });
  let cleanup = await installPersistTestEngines();
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    cleanup();
  });
});

add_task(async function no_keyboard_trap() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  let leftElement = window.document.getElementById(
    "urlbar-searchmode-switcher"
  );
  let inputField = gURLBar.inputField;
  let revertButton = gURLBar.querySelector(".urlbar-revert-button");
  let rightElement = window.document.getElementById("save-to-pocket-button");

  gURLBar.focus();

  Assert.equal(
    Services.focus.focusedElement,
    inputField,
    "Urlbar input field is focused."
  );

  info("Press Shift + Tab.");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  Assert.equal(
    Services.focus.focusedElement,
    leftElement,
    "Element left of input field is focused."
  );

  info("Press Tab.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    Services.focus.focusedElement,
    inputField,
    "Urlbar input field is focused."
  );

  info("Press Tab.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    Services.focus.focusedElement,
    revertButton,
    "Revert button is focused."
  );

  info("Press Tab.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    Services.focus.focusedElement,
    rightElement,
    "Save to Pocket Button is focused."
  );

  info("Press Shift + Tab.");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  Assert.equal(
    Services.focus.focusedElement,
    revertButton,
    "Revert button is focused."
  );

  info("Press Shift + Tab");
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  Assert.equal(
    Services.focus.focusedElement,
    inputField,
    "Urlbar input field is focused."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function enter_on_revert_keeps_focus_on_input_field() {
  let { tab } = await searchWithTab(SEARCH_STRING);

  gURLBar.focus();
  Assert.equal(
    Services.focus.focusedElement,
    gURLBar.inputField,
    "Input field is focused."
  );

  info("Press Tab.");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(
    Services.focus.focusedElement,
    gURLBar.querySelector(".urlbar-revert-button"),
    "Revert button is focused."
  );

  info("Press Tab.");
  EventUtils.synthesizeKey("KEY_Enter");
  Assert.equal(
    Services.focus.focusedElement,
    gURLBar.inputField,
    "Input field is focused."
  );

  BrowserTestUtils.removeTab(tab);
});
