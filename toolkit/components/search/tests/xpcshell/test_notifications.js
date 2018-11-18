/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestLog = [];

/**
 * The order of notifications expected for this test is:
 *  - engine-changed (while we're installing the engine, we modify it, which notifies - bug 606886)
 *  - engine-added (engine was added to the store by the search service)
 *   -> our search observer is called, which sets:
 *    - .defaultEngine, triggering engine-default
 *    - .currentEngine, triggering engine-current (after bug 493051 - for now the search service sets this after engine-added)
 *   ...and then schedules a removal
 *  - engine-loaded (the search service's observer is garanteed to fire first, which is what causes engine-added to fire)
 *  - engine-removed (due to the removal schedule above)
 */
var expectedLog = [
  "engine-changed", // XXX bug 606886
  "engine-added",
  "engine-default",
  "engine-current",
  "engine-loaded",
  "engine-removed",
];

function search_observer(subject, topic, data) {
  let engine = subject.QueryInterface(Ci.nsISearchEngine);
  gTestLog.push(data + " for " + engine.name);

  info("Observer: " + data + " for " + engine.name);

  switch (data) {
    case "engine-added":
      let retrievedEngine = Services.search.getEngineByName("Test search engine");
      Assert.equal(engine, retrievedEngine);
      Services.search.defaultEngine = engine;
      Services.search.defaultEngine = engine;
      executeSoon(function() {
        Services.search.removeEngine(engine);
      });
      break;
    case "engine-removed":
      let engineNameOutput = " for Test search engine";
      expectedLog = expectedLog.map(logLine => logLine + engineNameOutput);
      info("expectedLog:\n" + expectedLog.join("\n"));
      info("gTestLog:\n" + gTestLog.join("\n"));
      for (let i = 0; i < expectedLog.length; i++) {
        Assert.equal(gTestLog[i], expectedLog[i]);
      }
      Assert.equal(gTestLog.length, expectedLog.length);
      do_test_finished();
      break;
  }
}

function run_test() {
  useHttpServer();

  registerCleanupFunction(function cleanup() {
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  do_test_pending();

  Services.obs.addObserver(search_observer, "browser-search-engine-modified");

  Services.search.addEngine(gDataUrl + "engine.xml", null, false);
}
