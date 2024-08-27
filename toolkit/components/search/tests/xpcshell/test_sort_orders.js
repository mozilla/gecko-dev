/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check the correct default engines are picked from the configuration list,
 * and have the correct orders.
 */

"use strict";

// The expected order everywhere apart from the `gd` locale.
const EXPECTED_DEFAULT_ORDER = [
  "engine1",
  "engine6",
  "engine2",
  "engine3",
  "engine5",
  "engine4",
];

// The expected order when the `gd` locale is applied.
const EXPECTED_GD_ORDER = [
  "engine1",
  "engine4",
  "engine6",
  "engine2",
  "engine3",
  "engine5",
];
const CONFIG = [
  { identifier: "engine1" },
  { identifier: "engine2" },
  { identifier: "engine3" },
  { identifier: "engine4" },
  { identifier: "engine5" },
  { identifier: "engine6" },
  {
    orders: [
      {
        environment: { allRegionsAndLocales: true },
        order: EXPECTED_DEFAULT_ORDER,
      },
      {
        environment: { locales: ["gd"] },
        order: EXPECTED_GD_ORDER,
      },
    ],
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "gd",
  ];

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
});

async function checkOrder(type, expectedOrder) {
  // Reset the sorted list.
  Services.search.wrappedJSObject._cachedSortedEngines = null;

  const sortedEngines = await Services.search[type]();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    `Should have the expected engine order from ${type}`
  );
}

add_task(async function test_engine_sort_only_builtins() {
  await checkOrder("getAppProvidedEngines", EXPECTED_DEFAULT_ORDER);
  await checkOrder("getEngines", EXPECTED_DEFAULT_ORDER);
});

add_task(async function test_engine_sort_with_non_builtins_sort() {
  await SearchTestUtils.installSearchExtension({ name: "nonbuiltin1" });

  // As we've added an engine, the pref will have been set to true, but
  // we do really want to test the default sort.
  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "useSavedOrder",
    false
  );

  // We should still have the same built-in engines listed.
  await checkOrder("getAppProvidedEngines", EXPECTED_DEFAULT_ORDER);

  const expected = [...EXPECTED_DEFAULT_ORDER];
  expected.splice(EXPECTED_DEFAULT_ORDER.length, 0, "nonbuiltin1");
  await checkOrder("getEngines", expected);
});

add_task(async function test_engine_sort_with_locale() {
  await promiseSetLocale("gd");

  const expected = [...EXPECTED_GD_ORDER];

  await checkOrder("getAppProvidedEngines", expected);
  expected.push("nonbuiltin1");
  await checkOrder("getEngines", expected);
});
