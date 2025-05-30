/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  EnrollmentType: "resource://nimbus/ExperimentAPI.sys.mjs",
});

const { UrlbarProviderSemanticHistorySearch } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarProviderSemanticHistorySearch.sys.mjs"
);
const { getPlacesSemanticHistoryManager } = ChromeUtils.importESModule(
  "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs"
);

let semanticManager = getPlacesSemanticHistoryManager();
let hasSufficientEntriesStub = sinon
  .stub(semanticManager, "hasSufficientEntriesForSearching")
  .resolves(true);

add_task(async function setup() {
  registerCleanupFunction(() => {
    sinon.restore();
  });

  // stub getEnrollmentMetadata once, then configure for both cases:
  const getEnrollmentStub = sinon.stub(
    lazy.NimbusFeatures.urlbar,
    "getEnrollmentMetadata"
  );
  getEnrollmentStub
    .withArgs(lazy.EnrollmentType.EXPERIMENT)
    .returns({ slug: "test-slug", branch: "control" });
  getEnrollmentStub.withArgs(lazy.EnrollmentType.ROLLOUT).returns(null);
  sinon.stub(lazy.NimbusFeatures.urlbar, "recordExposureEvent");
});

add_task(async function test_startQuery_adds_results() {
  const provider = UrlbarProviderSemanticHistorySearch;

  // Set required prefs
  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs
    .getDefaultBranch("")
    .setIntPref("browser.urlbar.suggest.semanticHistory.minLength", 5);

  const queryContext = { searchString: "test page" };

  // Trigger isActive() to initialize the semantic manager
  Assert.ok(await provider.isActive(queryContext), "Provider should be active");

  // Stub and simulate inference
  sinon.stub(semanticManager.embedder, "ensureEngine").callsFake(() => {});
  let url = "https://example.com";
  sinon.stub(semanticManager, "infer").resolves({
    results: [
      {
        id: 1,
        title: "Test Page",
        url,
      },
    ],
  });
  await PlacesTestUtils.addVisits(url);

  let added = [];
  await provider.startQuery(queryContext, (_provider, result) => {
    added.push(result);
  });

  Assert.equal(added.length, 1, "One result should be added");
  Assert.equal(added[0].payload.url, url, "Correct URL should be used");
  Assert.equal(
    added[0].payload.icon,
    UrlbarUtils.getIconForUrl(url),
    "Correct icon should be used"
  );
  Assert.ok(added[0].payload.isBlockable, "Result should be blockable");

  let controller = UrlbarTestUtils.newMockController();
  let stub = sinon.stub(controller, "removeResult");
  let promiseRemoved = PlacesTestUtils.waitForNotification("page-removed");
  await provider.onEngagement(queryContext, controller, {
    selType: "dismiss",
    result: { payload: { url } },
  });
  Assert.ok(stub.calledOnce, "Result should be removed on dismissal");
  await promiseRemoved;
  let visited = await PlacesUtils.history.hasVisits(url);
  Assert.ok(!visited, "URL should have been removed from history");
});

add_task(async function test_isActive_conditions() {
  const provider = UrlbarProviderSemanticHistorySearch;

  // Stub canUseSemanticSearch to control the return value
  const canUseStub = sinon.stub(semanticManager, "canUseSemanticSearch");

  // Default settings
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setIntPref(
    "browser.urlbar.suggest.semanticHistory.minLength",
    5
  );

  const shortQuery = { searchString: "hi" };
  const validQuery = { searchString: "hello world" };

  // Pref is disabled
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Assert.ok(
    !(await provider.isActive(validQuery)),
    "Should be inactive when pref is disabled"
  );

  // Pref enabled, but string too short
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Assert.ok(
    !(await provider.isActive(shortQuery)),
    "Should be inactive for short search strings"
  );

  // All conditions met but semanticManager rejects
  canUseStub.get(() => false);
  hasSufficientEntriesStub.resetHistory();
  Assert.ok(
    !(await provider.isActive(validQuery)),
    "Should be inactive if canUseSemanticSearch returns false"
  );
  Assert.ok(
    hasSufficientEntriesStub.notCalled,
    "hasSufficientEntriesForSearching should not have been called"
  );

  // All conditions met
  canUseStub.get(() => true);
  Assert.ok(
    await provider.isActive(validQuery),
    "Should be active when all conditions are met"
  );

  const engineSearchMode = createContext("hello world", {
    searchMode: { engineName: "testEngine" },
  });
  Assert.ok(
    !(await provider.isActive(engineSearchMode)),
    "Should not be active when in search engine mode"
  );

  const historySearchMode = createContext("hello world", {
    searchMode: { source: UrlbarUtils.RESULT_SOURCE.HISTORY },
  });
  Assert.ok(
    await provider.isActive(historySearchMode),
    "Should be active when in history search mode"
  );
});
