/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
  Tests the showSearchTerms option on the about:preferences#search page.
*/

"use strict";

ChromeUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/QuickSuggestTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

const CHECKBOX_ID = "searchShowSearchTermCheckbox";
const PREF_SEARCHTERMS = "browser.urlbar.showSearchTerms.enabled";
const PREF_FEATUREGATE = "browser.urlbar.showSearchTerms.featureGate";
const PREF_SCOTCH_BONNET = "browser.urlbar.scotchBonnet.enableOverride";

add_task(async function showSearchTermsVisibility_scotchBonnet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_SCOTCH_BONNET, false]],
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#search",
    async browser => {
      let container = browser.contentDocument.getElementById(CHECKBOX_ID);
      Assert.ok(
        !BrowserTestUtils.isVisible(container),
        "The option box is not visible"
      );
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_SCOTCH_BONNET, true]],
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#search",
    async browser => {
      let container = browser.contentDocument.getElementById(CHECKBOX_ID);
      Assert.ok(
        BrowserTestUtils.isVisible(container),
        "The option box is visible"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
});

// To avoid impacting users who could be using Persisted Search but not Scotch
// Bonnet, deprecate the feature gate preference only after Scotch Bonnet is
// enabled by default.
add_task(async function showSearchTermsVisibility_featureGate() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, false]],
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#search",
    async browser => {
      let container = browser.contentDocument.getElementById(CHECKBOX_ID);
      Assert.ok(
        !BrowserTestUtils.isVisible(container),
        "The option box is not visible"
      );
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });

  await BrowserTestUtils.withNewTab(
    "about:preferences#search",
    async browser => {
      let container = browser.contentDocument.getElementById(CHECKBOX_ID);
      Assert.ok(
        BrowserTestUtils.isVisible(container),
        "The option box is visible"
      );
    }
  );

  await SpecialPowers.popPrefEnv();
  await SpecialPowers.popPrefEnv();
});

/*
  Check using the checkbox modifies the preference.
*/
add_task(async function showSearchTerms_checkbox() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let option = doc.getElementById(CHECKBOX_ID);

  // Evaluate checkbox pref is true.
  Assert.ok(option.checked, "Option box should be checked.");

  // Evaluate checkbox when pref is false.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_SEARCHTERMS, false]],
  });
  Assert.ok(!option.checked, "Option box should not be checked.");
  await SpecialPowers.popPrefEnv();

  // Evaluate pref when checkbox is un-checked.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_SEARCHTERMS),
    false,
    "Preference should be false if un-checked."
  );

  // Evaluate pref when checkbox is checked.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_SEARCHTERMS),
    true,
    "Preference should be true if checked."
  );

  // Clean-up.
  Services.prefs.clearUserPref(PREF_SEARCHTERMS);
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/*
  When loading the search preferences panel, the
  showSearchTerms checkbox should be hidden if
  the search bar is enabled.
*/
add_task(async function showSearchTerms_and_searchBar_preference_load() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });
  await gCUITestUtils.addSearchBar();

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.ok(
    checkbox.hidden,
    "showSearchTerms checkbox should be hidden when search bar is enabled."
  );

  // Clean-up.
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
  gCUITestUtils.removeSearchBar();
});

/*
  If the search bar is enabled while the search
  preferences panel is open, the showSearchTerms
  checkbox should not be clickable.
*/
add_task(async function showSearchTerms_and_searchBar_preference_change() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.ok(!checkbox.hidden, "showSearchTerms checkbox should be shown.");

  await gCUITestUtils.addSearchBar();
  Assert.ok(
    checkbox.hidden,
    "showSearchTerms checkbox should be hidden when search bar is enabled."
  );

  // Clean-up.
  gCUITestUtils.removeSearchBar();
  Assert.ok(!checkbox.hidden, "showSearchTerms checkbox should be shown.");

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});
