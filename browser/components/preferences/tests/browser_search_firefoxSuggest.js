/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Search pane's Firefox Suggest UI.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
});

const CONTAINER_ID = "firefoxSuggestContainer";
const NONSPONSORED_CHECKBOX_ID = "firefoxSuggestNonsponsored";
const SPONSORED_CHECKBOX_ID = "firefoxSuggestSponsored";
const DATA_COLLECTION_TOGGLE_ID = "firefoxSuggestDataCollectionSearchToggle";
const INFO_BOX_ID = "firefoxSuggestInfoBox";
const INFO_TEXT_ID = "firefoxSuggestInfoText";
const LEARN_MORE_CLASS = "firefoxSuggestLearnMore";
const BUTTON_RESTORE_DISMISSED_ID = "restoreDismissedSuggestions";
const PREF_URLBAR_QUICKSUGGEST_BLOCKLIST =
  "browser.urlbar.quicksuggest.blockedDigests";
const PREF_URLBAR_WEATHER_USER_ENABLED = "browser.urlbar.suggest.weather";

// This test can take a while due to the many permutations some of these tasks
// run through, so request a longer timeout.
requestLongerTimeout(10);

// The following tasks check the initial visibility of the Firefox Suggest UI
// and the visibility after installing a Nimbus experiment.

add_task(async function history_suggestDisabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["history"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: false,
    },
  });
});

add_task(async function history_suggestEnabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["history"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
    },
    newExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
  });
});

add_task(async function history_suggestEnabled_hideSettingsUIDisabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["history"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestHideSettingsUI: false,
    },
    newExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
  });
});

add_task(async function history_suggestEnabled_hideSettingsUIEnabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["history"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestHideSettingsUI: true,
    },
  });
});

add_task(async function offlineOnline_suggestDisabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: false,
    },
    newExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
  });
});

add_task(async function offlineOnline_suggestEnabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
    },
  });
});

add_task(async function offlineOnline_hideSettingsUIDisabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestHideSettingsUI: false,
    },
  });
});

add_task(async function offlineOnline_hideSettingsUIEnabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestHideSettingsUI: true,
    },
    newExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
  });
});

add_task(async function offlineOnline_suggestEnabled_hideSettingsUIDisabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestHideSettingsUI: false,
    },
  });
});

add_task(async function offlineOnline_suggestEnabled_hideSettingsUIEnabled() {
  await doSuggestVisibilityTest({
    initialScenarios: ["offline", "online"],
    initialExpected: {
      [CONTAINER_ID]: { isVisible: true },
      locationBarGroupHeader: {
        isVisible: true,
        l10nId: "addressbar-header-firefox-suggest",
      },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest-firefox-suggest",
      },
    },
    nimbusVariables: {
      quickSuggestEnabled: true,
      quickSuggestHideSettingsUI: true,
    },
    newExpected: {
      [CONTAINER_ID]: { isVisible: false },
      locationBarGroupHeader: { isVisible: true, l10nId: "addressbar-header" },
      locationBarSuggestionLabel: {
        isVisible: true,
        l10nId: "addressbar-suggest",
      },
    },
  });
});

// Verifies all 8 states of the 3 checkboxes and their related info box states.
add_task(async function checkboxesAndInfoBox() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored-sponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = true
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-nonsponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = true
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: false,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = true
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-data");
  await SpecialPowers.popPrefEnv();

  // suggest.quicksuggest.nonsponsored = false
  // suggest.quicksuggest.sponsored = false
  // quicksuggest.dataCollection.enabled = false
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: false,
  });
  await assertInfoBox(null);
  await SpecialPowers.popPrefEnv();

  gBrowser.removeCurrentTab();
});

// Clicks each of the checkboxes and toggles and makes sure the prefs and info box are updated.
add_task(async function clickCheckboxesOrToggle() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  async function clickElement(id, eventName) {
    let element = doc.getElementById(id);
    let changed = BrowserTestUtils.waitForEvent(element, eventName);

    if (eventName == "toggle") {
      element = element.buttonEl;
    }

    EventUtils.synthesizeMouseAtCenter(
      element,
      {},
      gBrowser.selectedBrowser.contentWindow
    );
    await changed;
  }

  // Set initial state.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: true,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");

  // non-sponsored checkbox
  await clickElement(NONSPONSORED_CHECKBOX_ID, "command");
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored is false after clicking non-sponsored checkbox"
  );
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: true,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-sponsored-data");

  // sponsored checkbox
  await clickElement(SPONSORED_CHECKBOX_ID, "command");
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored remains false after clicking sponsored checkbox"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "suggest.quicksuggest.sponsored is false after clicking sponsored checkbox"
  );
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: true,
  });
  await assertInfoBox("addressbar-firefox-suggest-info-data");

  // data collection toggle
  await clickElement(DATA_COLLECTION_TOGGLE_ID, "toggle");
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.nonsponsored"
    ),
    "suggest.quicksuggest.nonsponsored remains false after clicking sponsored checkbox"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "suggest.quicksuggest.sponsored remains false after clicking data collection toggle"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.quicksuggest.dataCollection.enabled"
    ),
    "quicksuggest.dataCollection.enabled is false after clicking data collection toggle"
  );
  assertPrefUIState({
    [NONSPONSORED_CHECKBOX_ID]: false,
    [SPONSORED_CHECKBOX_ID]: false,
    [DATA_COLLECTION_TOGGLE_ID]: false,
  });
  await assertInfoBox(null);

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Clicks the learn-more links and checks the help page is opened in a new tab.
add_task(async function clickLearnMore() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  // Set initial state so that the info box and learn more link are shown.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", true],
    ],
  });
  await assertInfoBox("addressbar-firefox-suggest-info-all");

  let learnMoreLinks = doc.querySelectorAll(
    "#locationBarGroup ." + LEARN_MORE_CLASS
  );
  Assert.equal(
    learnMoreLinks.length,
    3,
    "Expected number of learn-more links are present"
  );
  for (let link of learnMoreLinks) {
    Assert.ok(
      BrowserTestUtils.isVisible(link),
      "Learn-more link is visible: " + link.id
    );
  }

  let prefsTab = gBrowser.selectedTab;
  for (let link of learnMoreLinks) {
    let tabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      QuickSuggest.HELP_URL
    );
    info("Clicking learn-more link: " + link.id);
    Assert.ok(link.id, "Sanity check: Learn-more link has an ID");
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "#" + link.id,
      {},
      gBrowser.selectedBrowser
    );
    info("Waiting for help page to load in a new tab");
    await tabPromise;
    gBrowser.removeCurrentTab();
    gBrowser.selectedTab = prefsTab;
  }

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
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

/**
 * Verifies the state of pref related to checkboxes or toggles.
 *
 * @param {object} stateByElementID
 *   Maps checkbox or toggle element IDs to booleans. Each boolean
 *   is the expected state of the corresponding ID.
 */
function assertPrefUIState(stateByElementID) {
  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  let attr;
  Assert.ok(BrowserTestUtils.isVisible(container), "The container is visible");
  for (let [id, state] of Object.entries(stateByElementID)) {
    let element = doc.getElementById(id);
    if (element.tagName === "checkbox") {
      attr = "checked";
    } else if (element.tagName === "html:moz-toggle") {
      attr = "pressed";
    }
    Assert.equal(element[attr], state, "Expected state for ID: " + id);
  }
}

/**
 * Verifies the state of the info box.
 *
 * @param {string} expectedL10nID
 *   The l10n ID of the string that should be visible in the info box, null if
 *   the info box should be hidden.
 */
async function assertInfoBox(expectedL10nID) {
  info("Checking info box with expected l10n ID: " + expectedL10nID);
  let doc = gBrowser.selectedBrowser.contentDocument;
  let infoBox = doc.getElementById(INFO_BOX_ID);
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.isVisible(infoBox) == !!expectedL10nID,
    "Waiting for expected info box visibility: " + !!expectedL10nID
  );

  let infoIcon = infoBox.querySelector(".info-icon");
  Assert.equal(
    BrowserTestUtils.isVisible(infoIcon),
    !!expectedL10nID,
    "The info icon is visible iff a description should be shown"
  );

  let learnMore = infoBox.querySelector("." + LEARN_MORE_CLASS);
  Assert.ok(learnMore, "Found the info box learn more link");
  Assert.equal(
    BrowserTestUtils.isVisible(learnMore),
    !!expectedL10nID,
    "The info box learn more link is visible iff a description should be shown"
  );

  if (expectedL10nID) {
    let infoText = doc.getElementById(INFO_TEXT_ID);
    Assert.equal(
      infoText.dataset.l10nId,
      expectedL10nID,
      "Info text has expected l10n ID"
    );
  }
}
