/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with places related actions (e.g. history/
 * bookmark selection).
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

const TEST_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

function searchInAwesomebar(value, win = window) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    waitForFocus,
    value,
    fireInputEvent: true,
  });
}

function assertSearchTelemetryEmpty(search_hist) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(
    !(SCALAR_URLBAR in scalars),
    `Should not have recorded ${SCALAR_URLBAR}`
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    undefined
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );
  let sapEvent = Glean.sap.counts.testGetValue();
  Assert.equal(sapEvent, null, "Should not have recorded any SAP events");

  // Also check events.
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  events = (events.parent || []).filter(
    e => e[1] == "navigation" && e[2] == "search"
  );
  Assert.deepEqual(
    events,
    [],
    "Should not have recorded any navigation search events"
  );
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();
  return {
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      // Turn autofill off.
      ["browser.urlbar.autoFill", false],
    ],
  });

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesUtils.keywords.insert({
    keyword: "get",
    url: TEST_URL + "?q=%s",
  });

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function () {
    await PlacesUtils.keywords.remove("get");
    Services.telemetry.canRecordExtended = oldCanRecord;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_history() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com",
      title: "example",
      transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
    },
  ]);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_history_adaptive() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_bookmark_without_history() {
  await PlacesUtils.history.clear();

  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://example.com",
    title: "example",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
  });

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  await PlacesUtils.bookmarks.remove(bm);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_bookmark_with_history() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://example.com",
    title: "example",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
  });

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  await PlacesUtils.bookmarks.remove(bm);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_keyword() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("get example");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_switchtab() {
  const histograms = snapshotHistograms();

  let homeTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:buildconfig"
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  let p = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  await searchInAwesomebar("about:buildconfig");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(homeTab);
});

add_task(async function test_visitURL() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("http://example.com/a/");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);

  BrowserTestUtils.removeTab(tab);
});
