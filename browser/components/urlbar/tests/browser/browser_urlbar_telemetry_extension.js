/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with extension actions.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

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
  // SEARCH_COUNTS should not contain any engine counts at all. The keys in this
  // histogram are search engine telemetry identifiers.
  Assert.deepEqual(
    Object.keys(search_hist.snapshot()),
    [],
    "SEARCH_COUNTS is empty"
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

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function () {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      omnibox: {
        keyword: "omniboxtest",
      },
    },
    background() {
      /* global browser */
      browser.omnibox.setDefaultSuggestion({
        description: "doit",
      });
      // Just do nothing for this test.
      browser.omnibox.onInputEntered.addListener(() => {});
      browser.omnibox.onInputChanged.addListener((text, suggest) => {
        suggest([]);
      });
    },
  });

  await extension.startup();

  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "omniboxtest ",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");

  assertSearchTelemetryEmpty(histograms.search_hist);

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
