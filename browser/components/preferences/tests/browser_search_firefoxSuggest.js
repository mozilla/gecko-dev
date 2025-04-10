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
  Assert.ok(
    !(await QuickSuggest.canClearDismissedSuggestions()),
    "Sanity check: This test expects canClearDismissedSuggestions to return false initially"
  );

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  let button = doc.getElementById(BUTTON_RESTORE_DISMISSED_ID);
  Assert.ok(button.disabled, "Restore button is disabled initially.");

  await QuickSuggest.blockedSuggestions.add("https://example.com/");

  Assert.ok(
    await QuickSuggest.canClearDismissedSuggestions(),
    "canClearDismissedSuggestions should return true after dismissing a suggestion"
  );
  Assert.ok(!button.disabled, "Restore button is enabled after blocking URL.");

  let clearPromise = TestUtils.topicObserved("quicksuggest-dismissals-cleared");
  button.click();
  await clearPromise;

  Assert.ok(
    await QuickSuggest.blockedSuggestions.isEmpty(),
    "blockedSuggestions.isEmpty() should return true after restoring dismissals"
  );
  Assert.ok(
    !(await QuickSuggest.canClearDismissedSuggestions()),
    "canClearDismissedSuggestions should return false after restoring dismissals"
  );
  Assert.ok(button.disabled, "Restore button is disabled after clicking it.");

  gBrowser.removeCurrentTab();
});
