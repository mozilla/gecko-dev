/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests browser.engagement.navigation.urlbar_persisted.
 */

"use strict";

const { SearchSERPTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/SearchSERPTelemetry.sys.mjs"
);

const SCALAR_URLBAR_PERSISTED =
  "browser.engagement.navigation.urlbar_persisted";

const SEARCH_STRING = "chocolate";

let testEngine;
add_setup(async () => {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.showSearchTerms.featureGate", true]],
  });

  let cleanup = await installPersistTestEngines();
  testEngine = Services.search.getEngineByName("Example");

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    Services.telemetry.clearScalars();
    Services.telemetry.clearEvents();
    cleanup();
  });
});

async function searchForString(searchString, tab) {
  info(`Search for string: ${searchString}.`);
  let [expectedSearchUrl] = UrlbarUtils.getSearchQueryUrl(
    testEngine,
    searchString
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    expectedSearchUrl
  );
  gURLBar.focus();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: searchString,
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");
  await browserLoadedPromise;
  info(`Loaded page: ${expectedSearchUrl}`);
  return expectedSearchUrl;
}

async function gotoUrl(url, tab) {
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    url
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  await browserLoadedPromise;
  info(`Loaded page: ${url}`);
  await TestUtils.waitForTick();
}

async function goBack(browser, url) {
  info(`Go back to ${url}`);
  let promise = TestUtils.waitForCondition(
    () => gBrowser.selectedBrowser?.currentURI?.spec == url,
    "Waiting for the expected page to load"
  );
  browser.goBack();
  await promise;
}

function assertScalarSearchEnter(number) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_URLBAR_PERSISTED,
    "search_enter",
    number
  );
}

function assertScalarDoesNotExist(scalar) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(!(scalar in scalars), scalar + " must not be recorded.");
}

// A user making a search after making a search should result
// in the telemetry being recorded.
add_task(async function search_after_search() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await searchForString(SEARCH_STRING, tab);

  // Scalar should not exist from a blank page, only when a search
  // is conducted from a default SERP.
  await assertScalarDoesNotExist(SCALAR_URLBAR_PERSISTED);

  // After the first search, we should expect the SAP to change
  // because the search term should show up on the SERP.
  await searchForString(SEARCH_STRING, tab);
  assertScalarSearchEnter(1);

  // Check search counts.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "Example.urlbar-persisted",
    1
  );

  BrowserTestUtils.removeTab(tab);
});

// A user going to a tab that contains a SERP should
// trigger the telemetry when conducting a search.
add_task(async function switch_to_tab_and_search() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await searchForString(SEARCH_STRING, tab1);

  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await gotoUrl("https://test1.example.com/", tab2);

  await BrowserTestUtils.switchTab(gBrowser, tab1);
  await searchForString(SEARCH_STRING, tab1);
  assertScalarSearchEnter(1);

  // Check search count.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "Example.urlbar-persisted",
    1
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// A user going back to a SERP and doing another search should
// record urlbar-persisted telemetry.
add_task(async function search_and_go_back_and_search_again() {
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let serpUrl = await searchForString(SEARCH_STRING, tab);
  await gotoUrl("https://test2.example.com/", tab);

  // Go back to the SERP.
  await goBack(tab.linkedBrowser, serpUrl);
  await assertScalarDoesNotExist(SCALAR_URLBAR_PERSISTED);

  // Then do a search.
  await searchForString(SEARCH_STRING, tab);
  assertScalarSearchEnter(1);

  // Check search count.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "Example.urlbar-persisted",
    1
  );

  BrowserTestUtils.removeTab(tab);
});
