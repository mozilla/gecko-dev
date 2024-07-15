/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that a delayed add-on manager start up does not affect the start up
// of the search service.

"use strict";

const CONFIG_V2 = [
  {
    recordType: "engine",
    identifier: "engine",
    base: {
      name: "Test search engine",
      urls: {
        search: {
          base: "https://www.google.com/search",
          params: [
            {
              name: "channel",
              searchAccessPoint: {
                addressbar: "fflb",
                contextmenu: "rcs",
              },
            },
          ],
          searchTermParamName: "q",
        },
        suggestions: {
          base: "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "engine",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(() => {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_initialization_delayed_addon_manager() {
  let stub = await SearchTestUtils.useTestEngines("data", null, CONFIG_V2);
  // Wait until the search service gets its configuration before starting
  // to initialise the add-on manager. This simulates the add-on manager
  // starting late which used to cause the search service to fail to load any
  // engines.
  stub.callsFake(() => {
    Services.tm.dispatchToMainThread(() => {
      AddonTestUtils.promiseStartupManager();
    });
    return CONFIG_V2;
  });

  await Services.search.init();

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Test engine shouldn't be the default anymore"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
      displayName: "Test search engine",
      loadPath: ["[app]engine@search.mozilla.org"],
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
  });
});
