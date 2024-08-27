/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that appDefaultEngine property is set and switches correctly.
 */

"use strict";

const CONFIG = [
  { identifier: "default-engine" },
  {
    identifier: "an-engine",
    variants: [{ environment: { regions: ["an"] } }],
  },
  { identifier: "tr-engine" },
  {
    specificDefaults: [
      {
        default: "an-engine",
        environment: { regions: ["an"] },
      },
      {
        default: "tr-engine",
        environment: { regions: ["tr"] },
      },
    ],
  },
];

add_setup(async function () {
  Region._setHomeRegion("an", false);
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

add_task(async function test_appDefaultEngine() {
  await Promise.all([Services.search.init(), promiseAfterSettings()]);
  Assert.equal(
    Services.search.appDefaultEngine.name,
    "an-engine",
    "Should have returned the correct app default engine"
  );
});

add_task(async function test_changeRegion() {
  // Now change the region, and check we get the correct default according to
  // the config file.

  // Note: the test could be done with changing regions or locales. The important
  // part is that the default engine is changing across the switch, and that
  // the engine is not the first one in the new sorted engines list.
  await promiseSetHomeRegion("tr");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    // Very important this default is not the first one in the list (which is
    // the next fallback if the config one can't be found).
    "tr-engine",
    "Should have returned the correct engine for the new locale"
  );
});
