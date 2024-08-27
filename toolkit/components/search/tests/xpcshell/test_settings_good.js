/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from good search settings.
 */

"use strict";

const { getAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const enginesSettings = {
  version: SearchUtils.SETTINGS_VERSION,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  metaData: {
    searchDefault: "Test search engine",
    searchDefaultHash: "TBD",
    // Intentionally in the past, but shouldn't actually matter for this test.
    searchDefaultExpir: 1567694909002,
    // We use the second engine here so that the user's default is set
    // to something different, and hence so that we exercise the appropriate
    // code paths.
    defaultEngineId: "other",
    defaultEngineIdHash: "TBD",
    visibleDefaultEngines: "appDefault,other",
    visibleDefaultEnginesHash: "TBD",
  },
  engines: [
    {
      _metaData: { alias: null },
      _isAppProvided: true,
      _name: "appDefault",
      id: "appDefault",
    },
    {
      _metaData: { alias: null },
      _isAppProvided: true,
      _name: "other",
      id: "other",
    },
  ],
};

add_setup(async function () {
  // Allow telemetry probes which may otherwise be disabled for some applications (e.g. Thunderbird)
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  SearchTestUtils.setRemoteSettingsConfig([
    { identifier: "appDefault" },
    { identifier: "other" },
  ]);
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  // We dynamically generate the hashes because these depend on the profile.
  enginesSettings.metaData.searchDefaultHash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.searchDefault
  );
  enginesSettings.metaData.defaultEngineIdHash =
    SearchUtils.getVerificationHash(enginesSettings.metaData.defaultEngineId);
  enginesSettings.metaData.visibleDefaultEnginesHash =
    SearchUtils.getVerificationHash(
      enginesSettings.metaData.visibleDefaultEngines
    );
  const appInfo = getAppInfo();
  enginesSettings.buildID = appInfo.platformBuildID;
  enginesSettings.appVersion = appInfo.version;

  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    enginesSettings,
    { compress: true }
  );
});

add_task(async function test_cached_engine_properties() {
  info("init search service");

  const initResult = await Services.search.init();

  info("init'd search service");
  Assert.ok(
    Components.isSuccessCode(initResult),
    "Should have successfully created the search service"
  );

  const engines = await Services.search.getEngines();
  Assert.equal(
    Services.search.defaultEngine.name,
    "other",
    "Should have the expected default engine"
  );
  Assert.deepEqual(
    engines.map(e => e.name),
    ["appDefault", "other"],
    "Should have the expected application provided engines"
  );
});
