/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kSearchEngineID1 = "ignorelist_test_engine1";
const kSearchEngineID2 = "ignorelist_test_engine2";
const kSearchEngineID3 = "ignorelist_test_engine3";
const kSearchEngineURL1 = "https://example.com/?ignore=true";
const kSearchEngineURL2 = "https://example.com/?IGNORE=TRUE";
const kSearchEngineURL3 = "https://example.com/";
const kExtensionID = "searchignore@mozilla.com";

async function simulateIgnoreListUpdate() {
  await RemoteSettings("hijack-blocklists").emit("sync", {
    data: {
      current: [
        {
          id: "load-paths",
          schema: 1553857697843,
          last_modified: 1553859483588,
          matches: ["[addon]searchignore@mozilla.com"],
        },
        {
          id: "submission-urls",
          schema: 1553857697843,
          last_modified: 1553859435500,
          matches: ["ignore=true"],
        },
      ],
    },
  });
}

add_setup(function () {
  Services.fog.initializeFOG();
});

add_task(async function test_ignoreList() {
  Assert.ok(
    !Services.search.isInitialized,
    "Search service should not be initialized to begin with."
  );

  let updatePromise = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );
  // We skip unloading this extension because it's used in another task.
  await SearchTestUtils.installSearchExtension(
    {
      name: kSearchEngineID1,
      search_url: kSearchEngineURL1,
    },
    { skipUnload: true }
  );
  await SearchTestUtils.installSearchExtension({
    name: kSearchEngineID2,
    search_url: kSearchEngineURL2,
  });
  await SearchTestUtils.installSearchExtension({
    id: kExtensionID,
    name: kSearchEngineID3,
    search_url: kSearchEngineURL3,
  });

  // Ensure that the initial remote settings update from default values is
  // complete. The defaults do not include the special inclusions inserted below.
  await updatePromise;

  for (let engineName of [
    kSearchEngineID1,
    kSearchEngineID2,
    kSearchEngineID3,
  ]) {
    Assert.ok(
      await Services.search.getEngineByName(engineName),
      `Engine ${engineName} should be present`
    );
  }

  await simulateIgnoreListUpdate();

  for (let engineName of [
    kSearchEngineID1,
    kSearchEngineID2,
    kSearchEngineID3,
  ]) {
    Assert.equal(
      await Services.search.getEngineByName(engineName),
      null,
      `Engine ${engineName} should not be present`
    );
  }
});

add_task(async function test_correct_default_engine_change_reason() {
  Services.search.wrappedJSObject.reset();
  await Promise.all([Services.search.init(), promiseAfterSettings()]);

  await SearchTestUtils.installSearchExtension(
    {
      name: kSearchEngineID1,
      search_url: kSearchEngineURL1,
    },
    { setAsDefault: true }
  );
  Assert.ok(
    await Services.search.getEngineByName(kSearchEngineID1),
    "Engine ignorelist_test_engine1 should be present"
  );
  Assert.equal(
    await Services.search.getDefaultEngineInfo().defaultSearchEngine,
    `other-${kSearchEngineID1}`,
    "Engine ignorelist_test_engine1 should be the default search engine"
  );

  // Setting the extension as the default engine generates a default changed
  // event, but that is not the event we care about in this task.
  Services.fog.testResetFOG();

  await simulateIgnoreListUpdate();
  Assert.equal(
    await Services.search.getEngineByName(kSearchEngineID1),
    null,
    "Engine ignorelist_test_engine1 should not be present"
  );

  let snapshot = Glean.searchEngineDefault.changed.testGetValue();
  Assert.equal(
    snapshot[0].extra.change_reason,
    "ignore-list",
    "Ignore list update should have triggered default changed event"
  );
});
