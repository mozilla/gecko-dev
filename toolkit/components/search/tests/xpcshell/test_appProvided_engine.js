/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that ensure application provided engines have all base fields set up
 * correctly from the search configuration.
 */

"use strict";

let CONFIG = [
  {
    identifier: "testEngine",
    base: {
      aliases: ["testEngine1", "testEngine2"],
      charset: "EUC-JP",
      classification: "general",
      name: "testEngine name",
      partnerCode: "pc",
      urls: {
        search: {
          base: "https://example.com/1",
          // Method defaults to GET
          params: [
            { name: "partnerCode", value: "{partnerCode}" },
            { name: "starbase", value: "Regula I" },
            { name: "experiment", value: "Genesis" },
          ],
          searchTermParamName: "search",
        },
        suggestions: {
          base: "https://example.com/2",
          method: "POST",
          searchTermParamName: "suggestions",
        },
        trending: {
          base: "https://example.com/3",
          searchTermParamName: "trending",
        },
      },
    },
  },
  {
    identifier: "testOtherValuesEngine",
    base: {
      classification: "unknown",
      name: "testOtherValuesEngine name",
      urls: {
        search: {
          base: "https://example.com/1",
          searchTermParamName: "search",
        },
        trending: {
          base: "https://example.com/3",
        },
      },
    },
  },
  {
    identifier: "override",
    recordType: "engine",
    base: {
      classification: "unknown",
      name: "override name",
      urls: {
        search: {
          base: "https://www.example.com/search",
          params: [
            {
              name: "old_param",
              value: "old_value",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: {
          locales: ["en-US"],
        },
      },
    ],
  },
];

const TEST_CONFIG_OVERRIDE = [
  {
    identifier: "override",
    urls: {
      search: {
        params: [{ name: "new_param", value: "new_value" }],
      },
    },
    telemetrySuffix: "tsfx",
    clickUrl: "https://example.org/somewhere",
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async function test_engine_with_all_params_set() {
  let engine = Services.search.getEngineById("testEngine");
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.name,
    "testEngine name",
    "Should have the correct engine name"
  );
  Assert.deepEqual(
    engine.aliases,
    ["@testEngine1", "@testEngine2"],
    "Should have the correct aliases"
  );
  Assert.ok(
    engine.isGeneralPurposeEngine,
    "Should be a general purpose engine"
  );
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "EUC-JP",
    "Should have the correct encoding"
  );

  let submission = engine.getSubmission("test");
  Assert.equal(
    submission.uri.spec,
    "https://example.com/1?partnerCode=pc&starbase=Regula%20I&experiment=Genesis&search=test",
    "Should have the correct search URL"
  );
  Assert.ok(!submission.postData, "Should not have postData for a GET url");

  let suggestSubmission = engine.getSubmission(
    "test",
    SearchUtils.URL_TYPE.SUGGEST_JSON
  );
  Assert.equal(
    suggestSubmission.uri.spec,
    "https://example.com/2",
    "Should have the correct suggestion URL"
  );
  Assert.equal(
    suggestSubmission.postData.data.data,
    "suggestions=test",
    "Should have the correct postData for a POST URL"
  );

  let trendingSubmission = engine.getSubmission(
    "",
    SearchUtils.URL_TYPE.TRENDING_JSON
  );
  Assert.equal(
    trendingSubmission.uri.spec,
    "https://example.com/3?trending=",
    "Should have the correct trending URL with search term parameter."
  );
  Assert.ok(
    !trendingSubmission.postData,
    "Should not have postData for a GET url"
  );
});

add_task(async function test_engine_with_some_params_set() {
  let engine = Services.search.getEngineById("testOtherValuesEngine");
  Assert.ok(engine, "Should have found the engine");

  Assert.equal(
    engine.name,
    "testOtherValuesEngine name",
    "Should have the correct engine name"
  );
  Assert.deepEqual(engine.aliases, [], "Should have no aliases");
  Assert.ok(
    !engine.isGeneralPurposeEngine,
    "Should not be a general purpose engine"
  );
  Assert.equal(
    engine.wrappedJSObject.queryCharset,
    "UTF-8",
    "Should default to UTF-8 charset"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://example.com/1?search=test",
    "Should have the correct search URL"
  );
  Assert.equal(
    engine.getSubmission("test", SearchUtils.URL_TYPE.SUGGEST_JSON),
    null,
    "Should not have a suggestions URL"
  );
  let trendingSubmission = engine.getSubmission(
    "",
    SearchUtils.URL_TYPE.TRENDING_JSON
  );
  Assert.equal(
    trendingSubmission.uri.spec,
    "https://example.com/3",
    "Should have the correct trending URL without search term parameter."
  );
});

add_task(async function test_engine_remote_override() {
  // First check the existing engine doesn't have the overrides.
  let engine = Services.search.getEngineById("override");
  Assert.ok(engine, "Should have found the override engine");

  Assert.equal(engine.name, "override name", "Should have the expected name");
  Assert.equal(
    engine.telemetryId,
    "override",
    "Should have the overridden telemetry suffix"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://www.example.com/search?old_param=old_value&q=test",
    "Should have the overridden URL"
  );
  Assert.equal(engine.clickUrl, null, "Should not have a click URL");

  // Now apply and test the overrides.
  await SearchTestUtils.updateRemoteSettingsConfig(
    CONFIG,
    TEST_CONFIG_OVERRIDE
  );

  engine = Services.search.getEngineById("override");
  Assert.ok(engine, "Should have found the override engine");

  Assert.equal(engine.name, "override name", "Should have the expected name");
  Assert.equal(
    engine.telemetryId,
    "override-tsfx",
    "Should have the overridden telemetry suffix"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://www.example.com/search?new_param=new_value&q=test",
    "Should have the overridden URL"
  );
  Assert.equal(
    engine.clickUrl,
    "https://example.org/somewhere",
    "Should have the click URL specified by the override"
  );
});
