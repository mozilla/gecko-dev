/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that the ignore list is handled correctly when loading
 * settings and when installing engines.
 */

"use strict";

const kSearchEngineID1 = "ignorelist_test_engine1";
const kSearchEngineID2 = "ignorelist_test_engine2";
const kSearchEngineID3 = "ignorelist_test_engine3";
const kSearchEngineURL1 =
  "https://example.com/?search={searchTerms}&ignore=true";
const kSearchEngineURL2 =
  "https://example.com/?search={searchTerms}&IGNORE=TRUE";
const kSearchEngineURL3 = "https://example.com/?search={searchTerms}";
const kExtensionID = "searchignore@mozilla.com";

add_setup(async () => {
  const settings = await RemoteSettings("hijack-blocklists");
  // Set up a couple of items in the ignore list.
  sinon.stub(settings, "get").returns([
    {
      id: "load-paths",
      matches: ["[addon]searchignore@mozilla.com"],
      _status: "synced",
    },
    {
      id: "submission-urls",
      matches: ["ignore=true", "opensearch=sng"],
      _status: "synced",
    },
  ]);

  SearchTestUtils.setRemoteSettingsConfig([{ identifier: "defaultEngine" }]);
});

add_task(async function test_ignoreListOnLoadSettings() {
  consoleAllowList.push("Failed to load");

  Assert.ok(
    !Services.search.isInitialized,
    "Search service should not be initialized to begin with for this sub test"
  );

  let settingsTemplate = await readJSONFile(
    do_get_file("settings/ignorelist.json")
  );

  await promiseSaveSettingsData(settingsTemplate);

  let ignoreListUpdateCompleted = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );

  await Services.search.init();

  await ignoreListUpdateCompleted;

  Assert.ok(
    !(await Services.search.getEngineByName("Test search engine")),
    "Should not have installed the add-on engine from settings"
  );

  Assert.ok(
    !(await Services.search.getEngineByName("OpenSearchTest")),
    "Should not have installed the OpenSearch engine from settings"
  );

  Assert.deepEqual(
    (await Services.search.getEngines()).map(e => e.id),
    ["defaultEngine"],
    "Should have correctly started and installed only the default engine"
  );
});

add_task(async function test_ignoreListOnInstall() {
  Assert.ok(
    Services.search.isInitialized,
    "Search service should have been initialized to begin with for this sub test"
  );

  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID1,
    search_url: kSearchEngineURL1,
  });

  let engine = Services.search.getEngineByName(kSearchEngineID1);
  Assert.equal(
    engine,
    null,
    "Engine with ignored search params should not exist"
  );

  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID2,
    search_url: kSearchEngineURL2,
  });

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID2);
  Assert.equal(
    engine,
    null,
    "Engine with ignored search params of a different case should not exist"
  );

  await SearchTestUtils.installSearchExtension({
    id: kExtensionID,
    name: kSearchEngineID3,
    search_url: kSearchEngineURL3,
  });

  // An ignored engine shouldn't be available at all
  engine = Services.search.getEngineByName(kSearchEngineID3);
  Assert.equal(
    engine,
    null,
    "Engine with ignored extension id should not exist"
  );
});
