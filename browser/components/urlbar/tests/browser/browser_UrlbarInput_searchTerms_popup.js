/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These tests check the behavior of the Urlbar when persist search terms
// are either enabled or disabled, and a popup notification is shown.

function waitForPopupNotification() {
  let promisePopupShown = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  PopupNotifications.show(
    gBrowser.selectedBrowser,
    "test-notification",
    "This is a sample popup."
  );
  return promisePopupShown;
}

// The main search string used in tests.
const SEARCH_TERM = "chocolate";
const PREF_FEATUREGATE = "browser.urlbar.showSearchTerms.featureGate";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });
  let cleanup = await installPersistTestEngines();
  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    cleanup();
  });
});

// A notification should cause the urlbar to revert while
// the search term persists.
add_task(async function generic_popup_when_persist_is_enabled() {
  let { tab, expectedSearchUrl } = await searchWithTab(SEARCH_TERM);

  await waitForPopupNotification();

  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "Urlbar should have a valid pageproxystate."
  );

  Assert.equal(
    gURLBar.value,
    UrlbarTestUtils.trimURL(expectedSearchUrl),
    "Search url should be in the urlbar."
  );

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// Ensure the urlbar is not being reverted when a prompt is shown
// and the persist feature is disabled.
add_task(async function generic_popup_no_revert_when_persist_is_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, false]],
  });

  let { tab } = await searchWithTab(
    SEARCH_TERM,
    null,
    Services.search.defaultEngine,
    false
  );

  // Have a user typed value in the urlbar to make
  // pageproxystate invalid.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SEARCH_TERM,
  });
  gURLBar.blur();

  await waitForPopupNotification();

  // Wait a brief amount of time between when the popup is shown
  // and when the event handler should fire if it's enabled.
  await TestUtils.waitForTick();

  Assert.equal(
    gURLBar.getAttribute("pageproxystate"),
    "invalid",
    "Urlbar should not be reverted."
  );

  Assert.equal(
    gURLBar.value,
    SEARCH_TERM,
    "User typed value should remain in urlbar."
  );

  BrowserTestUtils.removeTab(tab);
  SpecialPowers.popPrefEnv();
});
