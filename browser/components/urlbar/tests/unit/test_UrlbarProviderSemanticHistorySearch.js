/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  EnrollmentType: "resource://nimbus/ExperimentAPI.sys.mjs",
});

const { UrlbarProviderSemanticHistorySearch } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarProviderSemanticHistorySearch.sys.mjs"
);
let sandbox;

add_task(async function setup() {
  sandbox = lazy.sinon.createSandbox();

  const { PlacesSemanticHistoryManager } = ChromeUtils.importESModule(
    "resource://gre/modules/PlacesSemanticHistoryManager.sys.mjs"
  );

  sandbox
    .stub(
      PlacesSemanticHistoryManager.prototype,
      "hasSufficientEntriesForSearching"
    )
    .returns(true);

  // stub getEnrollmentMetadata once, then configure for both cases:
  const getEnrollmentStub = sandbox.stub(
    lazy.NimbusFeatures.urlbar,
    "getEnrollmentMetadata"
  );
  getEnrollmentStub
    .withArgs(lazy.EnrollmentType.EXPERIMENT)
    .returns({ slug: "test-slug", branch: "control" });
  getEnrollmentStub.withArgs(lazy.EnrollmentType.ROLLOUT).returns(null);
  sandbox.stub(lazy.NimbusFeatures.urlbar, "recordExposureEvent");
});

add_task(async function test_startQuery_adds_results() {
  const provider = UrlbarProviderSemanticHistorySearch;

  // Set required prefs
  Services.prefs.setBoolPref("browser.ml.enable", true);
  Services.prefs.setBoolPref("places.semanticHistory.featureGate", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.semanticHistory", true);
  Services.prefs
    .getDefaultBranch("")
    .setIntPref("browser.urlbar.suggest.semanticHistory.minLength", 5);

  const queryContext = { searchString: "test page" };

  // Trigger isActive() to initialize the semantic manager
  Assert.ok(await provider.isActive(queryContext), "Provider should be active");

  const semanticManager = provider.ensureSemanticManagerInitialized();

  // Stub and simulate inference
  sandbox.stub(semanticManager.embedder, "ensureEngine").callsFake(() => {});
  sandbox.stub(semanticManager, "infer").resolves({
    results: [
      {
        id: 1,
        title: "Test Page",
        url: "https://example.com",
        helpLink: "https://example.com/icon",
      },
    ],
  });

  let added = [];
  await provider.startQuery(queryContext, (_provider, result) => {
    added.push(result);
  });

  Assert.equal(added.length, 1, "One result should be added");
  Assert.equal(
    added[0].payload.url,
    "https://example.com",
    "Correct URL should be used"
  );
});

add_task(async function test_isActive_conditions() {
  const provider = UrlbarProviderSemanticHistorySearch;
  const semanticManager = provider.ensureSemanticManagerInitialized();

  // Stub canUseSemanticSearch to control the return value
  const canUseStub = sandbox.stub(semanticManager, "canUseSemanticSearch");

  // Default settings
  Services.prefs.setBoolPref("browser.urlbar.suggest.semanticHistory", true);
  Services.prefs.setIntPref(
    "browser.urlbar.suggest.semanticHistory.minLength",
    5
  );

  const shortQuery = { searchString: "hi" };
  const validQuery = { searchString: "hello world" };

  // Pref is disabled
  Services.prefs.setBoolPref("browser.urlbar.suggest.semanticHistory", false);
  Assert.ok(
    !(await provider.isActive(validQuery)),
    "Should be inactive when pref is disabled"
  );

  // Pref enabled, but string too short
  Services.prefs.setBoolPref("browser.urlbar.suggest.semanticHistory", true);
  Assert.ok(
    !(await provider.isActive(shortQuery)),
    "Should be inactive for short search strings"
  );

  // All conditions met but semanticManager rejects
  Services.prefs.setIntPref(
    "browser.urlbar.suggest.semanticHistory.minLength",
    5
  );
  canUseStub.get(() => false);
  Assert.ok(
    !(await provider.isActive(validQuery)),
    "Should be inactive if canUseSemanticSearch returns false"
  );

  // All conditions met
  canUseStub.get(() => true);
  Assert.ok(
    await provider.isActive(validQuery),
    "Should be active when all conditions are met"
  );
});

add_task(function cleanup() {
  sandbox.restore();
});
