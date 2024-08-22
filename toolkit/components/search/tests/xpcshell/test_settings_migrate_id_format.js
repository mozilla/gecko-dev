/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test migration of app provided engines to new ID format
 * (without @search.mozilla.orglocale).
 */

"use strict";

const SEARCH_SETTINGS = {
  version: 9,
  metaData: {
    useSavedOrder: true,
    defaultEngineId: "engine2@search.mozilla.orgdefault",
  },
  engines: [
    {
      id: "engine1@search.mozilla.orgde",
      _name: "engine1",
      _isAppProvided: true,
      _metaData: { order: 2 },
    },
    {
      id: "engine2@search.mozilla.orgdefault",
      _name: "engine2",
      _isAppProvided: true,
      _metaData: { order: 1 },
    },
    // It is possible for inactive engines to not have an id even if the
    // settings version is over 6, see bug 1914380.
    {
      _name: "engine3",
      _isAppProvided: true,
    },
  ],
};

const CONFIG = [{ identifier: "engine1-de" }, { identifier: "engine2" }];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);

  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    SEARCH_SETTINGS,
    { compress: true }
  );

  await Services.search.init();
});

add_task(async function test_cached_engine_properties() {
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine2",
    "Should have the expected default engine"
  );

  const engines = await Services.search.getEngines();
  Assert.deepEqual(
    engines.map(e => e.name),
    ["engine2", "engine1-de"],
    "Should have the expected application provided engines"
  );
});
