/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  useTestEngineConfig();
});

async function checkOrder(expectedOrder) {
  await asyncReInit();
  Assert.ok(Services.search.isInitialized, "search initialized");

  const sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    "Should have the expected engine order"
  );
}

add_task(async function test_searchOrderJSON_no_separate_private() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );

  // With modern configuration, we have a slightly different order, since the
  // default private engine will get placed second, regardless of if the
  // separate private engine is enabled or not.
  if (
    Services.prefs.getBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
      false
    )
  ) {
    await checkOrder([
      // Default engines
      "Test search engine",
      "engine-pref",
      // Two engines listed in searchOrder.
      "engine-resourceicon",
      "engine-chromeicon",
      // Rest of the engines in order.
      "engine-rel-searchform-purpose",
      "Test search engine (Reordered)",
    ]);
  } else {
    await checkOrder([
      // Default engine
      "Test search engine",
      // Two engines listed in searchOrder.
      "engine-resourceicon",
      "engine-chromeicon",
      // Rest of the engines in order.
      "engine-pref",
      "engine-rel-searchform-purpose",
      "Test search engine (Reordered)",
    ]);
  }
});

add_task(async function test_searchOrderJSON_separate_private() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  await checkOrder([
    // Default engine
    "Test search engine",
    // Default private engine
    "engine-pref",
    // Two engines listed in searchOrder.
    "engine-resourceicon",
    "engine-chromeicon",
    // Rest of the engines in order.
    "engine-rel-searchform-purpose",
    "Test search engine (Reordered)",
  ]);
});
