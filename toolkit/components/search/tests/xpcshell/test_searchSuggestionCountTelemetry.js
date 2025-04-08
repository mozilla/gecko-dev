/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether the suggestion count telemetry is correctly divided
 * into {successful, aborted, failed}, and whether it is only recorded
 * for app-provided search engines.
 */

"use strict";

const { SearchSuggestionController } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchSuggestionController.sys.mjs"
);

let openSearchEngine, workingAppEngine, failingAppEngine;

add_setup(async function () {
  // Initialize Glean.
  do_get_profile();
  Services.fog.initializeFOG();
  // Initialize sjs server early so we have access to gHttpURL.
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  consoleAllowList = consoleAllowList.concat([
    "SearchSuggestionController found an unexpected string value",
  ]);

  SearchTestUtils.setRemoteSettingsConfig([
    {
      identifier: "workingAppEngine",
      base: {
        urls: {
          suggestions: {
            base: `${gHttpURL}/sjs/searchSuggestions.sjs`,
            searchTermParamName: "q",
          },
        },
      },
    },
    {
      identifier: "failingAppEngine",
      base: {
        urls: {
          suggestions: {
            base: "http://example.invalid/",
            searchTermParamName: "q",
          },
        },
      },
    },
  ]);
  await Services.search.init();

  let openSearchEngineData = {
    baseURL: `${gHttpURL}/sjs/`,
    name: "GET suggestion engine",
    method: "GET",
  };

  openSearchEngine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/sjs/engineMaker.sjs?${JSON.stringify(openSearchEngineData)}`,
  });
  workingAppEngine = Services.search.getEngineById("workingAppEngine");
  failingAppEngine = Services.search.getEngineById("failingAppEngine");
});

add_task(async function test_success() {
  for (let i = 0; i < 5; i++) {
    let controller = new SearchSuggestionController();
    let result = await controller.fetch("mo", false, workingAppEngine);
    Assert.equal(result.remote.length, 3);
  }

  Assert.equal(
    Glean.searchSuggestions.successfulRequests.workingAppEngine.testGetValue(),
    5,
    "Successful request counter is correctly updated"
  );
});

add_task(async function test_abort() {
  let controller = new SearchSuggestionController();

  // Don't await the result to trigger the abort handler.
  controller.fetch("mo", false, workingAppEngine);
  controller.fetch("mo", false, workingAppEngine);
  controller.fetch("mo", false, workingAppEngine);
  controller.fetch("mo", false, workingAppEngine);
  await controller.fetch("mo", false, workingAppEngine);

  Assert.equal(
    Glean.searchSuggestions.successfulRequests.workingAppEngine.testGetValue(),
    6, // 1 new + 5 from the previous test.
    "Successful request counter is correctly updated"
  );
  Assert.equal(
    Glean.searchSuggestions.abortedRequests.workingAppEngine.testGetValue(),
    4,
    "Aborted request counter is correctly updated"
  );
});

add_task(async function test_error() {
  let controller = new SearchSuggestionController();
  await controller.fetch("mo", false, failingAppEngine);
  await controller.fetch("mo", false, failingAppEngine);
  await controller.fetch("mo", false, failingAppEngine);

  Assert.equal(
    Glean.searchSuggestions.failedRequests.failingAppEngine.testGetValue(),
    3,
    "Failed request counter is correctly updated"
  );
});

add_task(async function test_nonAppProvided() {
  let controller = new SearchSuggestionController();
  let result = await controller.fetch("mo", false, openSearchEngine);
  Assert.equal(result.remote.length, 3);

  Assert.equal(
    Glean.searchSuggestions.successfulRequests.openSearchEngine.testGetValue(),
    null,
    "No telemetry is recorded for non-app-provided-engine"
  );
});
