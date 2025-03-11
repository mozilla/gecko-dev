/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let engine;

add_setup(async function () {
  engine = await SearchTestUtils.installOpenSearchEngine({
    url: getRootDirectory(gTestPath) + "testEngine.xml",
    setAsDefault: true,
  });
  TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");
  Services.fog.testResetFOG();
});

add_task(async function test() {
  let histogramKey = "other-" + engine.name + ".contextmenu";

  let tabs = [];
  let tabsLoadedDeferred = new Deferred();

  function tabAdded(event) {
    let tab = event.target;
    tabs.push(tab);

    // We wait for the blank tab and the two context searches tabs to open.
    if (tabs.length == 3) {
      tabsLoadedDeferred.resolve();
    }
  }

  let container = gBrowser.tabContainer;
  container.addEventListener("TabOpen", tabAdded);

  BrowserTestUtils.addTab(gBrowser, "about:blank");

  SearchUIUtils.loadSearchFromContext(
    window,
    "mozilla",
    false,
    Services.scriptSecurityManager.getSystemPrincipal(),
    Services.scriptSecurityManager.getSystemPrincipal().csp,
    new PointerEvent("click")
  );
  SearchUIUtils.loadSearchFromContext(
    window,
    "firefox",
    false,
    Services.scriptSecurityManager.getSystemPrincipal(),
    Services.scriptSecurityManager.getSystemPrincipal().csp,
    new PointerEvent("click")
  );

  // Wait for all the tabs to open.
  await tabsLoadedDeferred.promise;

  is(tabs[0], gBrowser.tabs[3], "blank tab has been pushed to the end");
  is(
    tabs[1],
    gBrowser.tabs[1],
    "first search tab opens next to the current tab"
  );
  is(
    tabs[2],
    gBrowser.tabs[2],
    "second search tab opens next to the first search tab"
  );

  container.removeEventListener("TabOpen", tabAdded);
  tabs.forEach(gBrowser.removeTab, gBrowser);

  // Make sure that the context searches are correctly recorded in telemetry.
  // Telemetry is not updated synchronously here, we must wait for it.
  await TestUtils.waitForCondition(() => {
    let hs = Services.telemetry
      .getKeyedHistogramById("SEARCH_COUNTS")
      .snapshot();
    return histogramKey in hs && hs[histogramKey].sum == 2;
  }, "The histogram must contain the correct search count");

  let sapEvent = Glean.sap.counts.testGetValue();
  Assert.equal(
    sapEvent.length,
    2,
    "Should have recorded the correct number of searches"
  );
  Assert.deepEqual(
    sapEvent.map(e => e.extra),
    [
      {
        provider_id: "other",
        provider_name: "Foo",
        source: "contextmenu",
      },
      { provider_id: "other", provider_name: "Foo", source: "contextmenu" },
    ],
    "Should have the expected event telemetry data"
  );
});

function Deferred() {
  this.promise = new Promise((resolve, reject) => {
    this.resolve = resolve;
    this.reject = reject;
  });
}
