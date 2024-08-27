/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Integration type test to check that the search service uses the environment
 * when loading the search configuration.
 *
 * More extensive testing of the search configuration takes place in
 * test_engine_selector_environment.js.
 */

"use strict";

const CONFIG = [
  { identifier: "appDefault" },
  { identifier: "localeFR", variants: [{ environment: { locales: ["fr"] } }] },
  {
    identifier: "notDELocale",
    variants: [{ environment: { excludedLocales: ["de"] } }],
  },
  { identifier: "regionGB", variants: [{ environment: { regions: ["gb"] } }] },
  {
    globalDefault: "appDefault",
    specificDefaults: [
      { defaultPrivate: "localeFR", environment: { locales: ["fr"] } },
    ],
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "de",
    "fr",
  ];

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Region._setHomeRegion("US", false);
});

add_task(async function test_locale_selection() {
  Services.locale.requestedLocales = ["de"];

  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(sortedEngines.length, 1, "Should have only one engine");

  Assert.equal(
    Services.search.defaultEngine.identifier,
    "appDefault",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.identifier,
    // 'de' only displays google, so we'll be using the same engine as the
    // normal default.
    "appDefault",
    "Should have the correct private default engine"
  );
});

add_task(async function test_switch_locales() {
  await promiseSetLocale("fr");

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(e => e.name),
    ["appDefault", "localeFR", "notDELocale"],
    "Should have the correct engine list"
  );

  Assert.equal(
    Services.search.defaultEngine.identifier,
    "appDefault",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.identifier,
    "localeFR",
    "Should have the correct private default engine"
  );
});

add_task(async function test_region_selection() {
  await promiseSetHomeRegion("GB");

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(e => e.identifier),
    ["appDefault", "localeFR", "notDELocale", "regionGB"],
    "Should have the correct engine list"
  );

  Assert.equal(
    Services.search.defaultEngine.identifier,
    "appDefault",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.identifier,
    "localeFR",
    "Should have the correct private default engine"
  );
});
