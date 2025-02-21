/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests telemetry for tabtosearch results.
 * NB: This file does not test the search mode `entry` field for tab-to-search
 * results. That is tested in browser_UsageTelemetry_urlbar_searchmode.js.
 */

"use strict";

const ENGINE_NAME = "MozSearch";
const ENGINE_DOMAIN = "example.com";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.sys.mjs",
});

/**
 * Checks to see if the second result in the Urlbar is a tab-to-search result
 * with the correct engine.
 *
 * @param {string} engineName
 *   The expected engine name.
 * @param {boolean} [isOnboarding]
 *   If true, expects the tab-to-search result to be an onbarding result.
 */
async function checkForTabToSearchResult(engineName, isOnboarding) {
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open.");
  let tabToSearchResult = (
    await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
  ).result;
  Assert.equal(
    tabToSearchResult.providerName,
    "TabToSearch",
    "The second result is a tab-to-search result."
  );
  Assert.equal(
    tabToSearchResult.payload.engine,
    engineName,
    "The tab-to-search result is for the first engine."
  );
  if (isOnboarding) {
    Assert.equal(
      tabToSearchResult.payload.dynamicType,
      "onboardTabToSearch",
      "The tab-to-search result is an onboarding result."
    );
  } else {
    Assert.ok(
      !tabToSearchResult.payload.dynamicType,
      "The tab-to-search result should not be an onboarding result."
    );
  }
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0],
      ["browser.urlbar.scotchBonnet.enableOverride", false],
    ],
  });

  await SearchTestUtils.installSearchExtension({
    name: ENGINE_NAME,
    search_url: `https://${ENGINE_DOMAIN}/`,
  });

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits([`https://${ENGINE_DOMAIN}/`]);
    }
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ENGINE_DOMAIN.slice(0, 4),
      fireInputEvent: true,
    });

    let tabToSearchResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      tabToSearchResult.providerName,
      "TabToSearch",
      "The second result is a tab-to-search result."
    );
    Assert.equal(
      tabToSearchResult.payload.engine,
      ENGINE_NAME,
      "The tab-to-search result is for the correct engine."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      1,
      "Sanity check: The second result is selected."
    );

    // Select the tab-to-search result.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: ENGINE_NAME,
      entry: "tabtosearch",
    });

    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    await PlacesUtils.history.clear();
  });

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
});
