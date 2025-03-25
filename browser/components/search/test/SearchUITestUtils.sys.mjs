/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

/**
 * A class containing useful testing functions for Search UI based tests.
 */
export const SearchUITestUtils = new (class {
  /**
   * The test scope that the test is running in.
   *
   * @type {object}
   */
  #testScope = null;

  /**
   * Sets the scope for the test.
   *
   * @param {object} testScope
   *   The global scope for the test.
   */
  init(testScope) {
    this.#testScope = testScope;
  }

  /**
   * Asserts that the Search Access Point (SAP) telemetry is reported correctly.
   * It assumes that the reported telemetry is from a single source and no other
   * reports for the probes are expected.
   *
   * You may need to clear telemetry before running the test.
   *
   * @param {object} expected
   * @param {?string} expected.engineId
   *   The identifier of the simulated application provided search engine. If it
   *   is not an application provided engine, do not specify this value.
   * @param {?string} expected.engineName
   *   The name of the search engine.
   * @param {string} expected.source
   *   The source of the search (e.g. urlbar, contextmenu etc.).
   * @param {number} expected.count
   *   The expected count for the source.
   */
  async assertSAPTelemetry({
    engineId = null,
    engineName = "",
    source,
    count,
  }) {
    await lazy.TestUtils.waitForCondition(() => {
      return Glean.sap.counts.testGetValue().length == count;
    }, "The correct number of events should have been reported for sap.counts");

    let expectedEvents = [];
    for (let i = 0; i < count; i++) {
      expectedEvents.push({
        provider_id: engineId ?? "other",
        provider_name: engineName,
        source,
      });
    }

    let sapEvent = Glean.sap.counts.testGetValue();
    this.#testScope.Assert.deepEqual(
      sapEvent.map(e => e.extra),
      expectedEvents,
      "Should have the expected event telemetry data for sap.counts"
    );

    let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");

    let histogramKey = `${engineId ? "" : "other-"}${engineName}.${source}`;

    lazy.TelemetryTestUtils.assertKeyedHistogramSum(
      histogram,
      histogramKey,
      count
    );
    // Also ensure no other keys were set.
    let snapshot = histogram.snapshot();
    this.#testScope.Assert.deepEqual(
      Object.keys(snapshot),
      [histogramKey],
      "Should have only the expected key in the SEARCH_COUNTS histogram"
    );
  }
})();
