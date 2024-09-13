/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading engines when a user has an engine installed that is the
 * same name as an application provided engine being added to the user's set
 * of engines.
 *
 * Ensures that settings are not automatically taken across.
 */

"use strict";

const CONFIG = [
  {
    identifier: "appDefault",
    base: { name: "Application Default" },
  },
  {
    identifier: "notInFR",
    base: { name: "Not In FR" },
    variants: [{ environment: { regions: ["FR"] } }],
  },
];

add_setup(async () => {
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  // We use a region that doesn't install `engine-pref` by default so that we can
  // manually install it first (like when a user installs a browser add-on), and
  // then test what happens when we switch regions to one which would install
  // `engine-pref`.
  Region._setHomeRegion("US", false);

  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async function test_reload_engines_with_duplicate() {
  let engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.identifier),
    ["appDefault"],
    "Should have the expected default engines"
  );
  // Simulate a user installing a search engine that shares the same name as an
  // application provided search engine not currently installed in their browser.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/sjs/engineMaker.sjs?${JSON.stringify({
      baseURL: `${gHttpURL}/data/`,
      name: "Not In FR",
      method: "GET",
    })}`,
  });

  engine.alias = "testEngine";

  let engineId = engine.id;

  Region._setHomeRegion("FR", false);

  await Services.search.wrappedJSObject._maybeReloadEngines();

  Assert.ok(
    !(await Services.search.getEngineById(engineId)),
    "Should not have added the duplicate engine"
  );

  engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.identifier),
    ["appDefault", "notInFR"],
    "Should have the expected default engines"
  );

  let enginePref = await Services.search.getEngineByName("Not In FR");

  Assert.equal(
    enginePref.alias,
    "",
    "Should not have copied the alias from the duplicate engine"
  );
});
