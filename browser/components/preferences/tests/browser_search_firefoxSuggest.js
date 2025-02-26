/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Search pane's Firefox Suggest UI.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
});

const CONTAINER_ID = "firefoxSuggestContainer";
const DATA_COLLECTION_TOGGLE_ID = "firefoxSuggestDataCollectionSearchToggle";
const LEARN_MORE_ID = "firefoxSuggestLearnMore";
const BUTTON_RESTORE_DISMISSED_ID = "restoreDismissedSuggestions";
const PREF_URLBAR_QUICKSUGGEST_BLOCKLIST =
  "browser.urlbar.quicksuggest.blockedDigests";
const PREF_URLBAR_WEATHER_USER_ENABLED = "browser.urlbar.suggest.weather";

// Maps `SETTINGS_UI` values to expected visibility state objects. See
// `assertSuggestVisibility()` in `head.js` for info on the state objects.
const EXPECTED = {
  [QuickSuggest.SETTINGS_UI.FULL]: {
    [LEARN_MORE_ID]: { isVisible: true },
    [CONTAINER_ID]: { isVisible: true },
    [DATA_COLLECTION_TOGGLE_ID]: { isVisible: true },
    locationBarGroupHeader: {
      isVisible: true,
      l10nId: "addressbar-header-firefox-suggest",
    },
    locationBarSuggestionLabel: {
      isVisible: true,
      l10nId: "addressbar-suggest-firefox-suggest",
    },
  },
  [QuickSuggest.SETTINGS_UI.NONE]: {
    [LEARN_MORE_ID]: { isVisible: false },
    [CONTAINER_ID]: { isVisible: false },
    locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
    locationBarSuggestionLabel: {
      isVisible: true,
      l10nId: "addressbar-suggest",
    },
  },
  [QuickSuggest.SETTINGS_UI.OFFLINE_ONLY]: {
    [LEARN_MORE_ID]: { isVisible: true },
    [CONTAINER_ID]: { isVisible: true },
    [DATA_COLLECTION_TOGGLE_ID]: { isVisible: false },
    locationBarGroupHeader: {
      isVisible: true,
      l10nId: "addressbar-header-firefox-suggest",
    },
    locationBarSuggestionLabel: {
      isVisible: true,
      l10nId: "addressbar-suggest-firefox-suggest",
    },
  },
};

// This test can take a while due to the many permutations some of these tasks
// run through, so request a longer timeout.
requestLongerTimeout(10);

// The following tasks check the initial visibility of the Firefox Suggest UI
// and the visibility after installing a Nimbus experiment.

add_task(async function initiallyDisabled_disable() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: false,
    },
  });
});

add_task(async function initiallyDisabled_disable_settingsUiFull() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: false,
      // `quickSuggestEnabled: false` should override this, so the Suggest
      // settings should not be visible (`initialExpected` should persist).
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.FULL,
    },
  });
});

add_task(async function initiallyDisabled_enable() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: true,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
  });
});

add_task(async function initiallyDisabled_enable_settingsUiFull() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.FULL,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
  });
});

add_task(async function initiallyDisabled_enable_settingsUiNone() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.NONE,
    },
  });
});

add_task(async function initiallyDisabled_enable_settingsUiOfflineOnly() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: false,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.OFFLINE_ONLY,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.OFFLINE_ONLY],
  });
});

add_task(async function initiallyEnabled_disable() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestEnabled: false,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
  });
});

add_task(async function initiallyEnabled_disable_settingsUiFull() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestEnabled: false,
      // `quickSuggestEnabled: false` should override this, so the Suggest
      // settings should not be visible.
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.FULL,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
  });
});

add_task(async function initiallyEnabled_enable() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestEnabled: true,
    },
  });
});

add_task(async function initiallyEnabled_settingsUiFull() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.FULL,
    },
  });
});

add_task(async function initiallyEnabled_settingsUiNone() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.NONE,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.NONE],
  });
});

add_task(async function initiallyEnabled_settingsUiOfflineOnly() {
  await doSuggestVisibilityTest({
    initialSuggestEnabled: true,
    initialExpected: EXPECTED[QuickSuggest.SETTINGS_UI.FULL],
    nimbusVariables: {
      quickSuggestSettingsUi: QuickSuggest.SETTINGS_UI.OFFLINE_ONLY,
    },
    newExpected: EXPECTED[QuickSuggest.SETTINGS_UI.OFFLINE_ONLY],
  });
});

// Tests the "Restore" button for dismissed suggestions.
add_task(async function restoreDismissedSuggestions() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  let button = doc.getElementById(BUTTON_RESTORE_DISMISSED_ID);
  Assert.equal(
    Services.prefs.getStringPref(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST, ""),
    "",
    "Block list is empty initially"
  );
  Assert.ok(
    Services.prefs.getBoolPref(PREF_URLBAR_WEATHER_USER_ENABLED),
    "Weather suggestions are enabled initially"
  );
  Assert.ok(button.disabled, "Restore button is disabled initially.");

  await QuickSuggest.blockedSuggestions.add("https://example.com/");
  Assert.notEqual(
    Services.prefs.getStringPref(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST, ""),
    "",
    "Block list is non-empty after adding URL"
  );
  Assert.ok(!button.disabled, "Restore button is enabled after blocking URL.");
  button.click();
  Assert.equal(
    Services.prefs.getStringPref(PREF_URLBAR_QUICKSUGGEST_BLOCKLIST, ""),
    "",
    "Block list is empty clicking Restore button"
  );
  Assert.ok(button.disabled, "Restore button is disabled after clicking it.");

  Services.prefs.setBoolPref(PREF_URLBAR_WEATHER_USER_ENABLED, false);
  Assert.ok(
    !button.disabled,
    "Restore button is enabled after disabling weather suggestions."
  );
  button.click();
  Assert.ok(
    Services.prefs.getBoolPref(PREF_URLBAR_WEATHER_USER_ENABLED),
    "Weather suggestions are enabled after clicking Restore button"
  );
  Assert.ok(
    button.disabled,
    "Restore button is disabled after clicking it again."
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});
