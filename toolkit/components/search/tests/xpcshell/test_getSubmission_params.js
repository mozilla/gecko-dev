/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_SEARCH_TERM = "civilisations";

const TESTS = [
  // The same parameter can be used more than once.
  {
    param: "{searchTerms}/{searchTerms}",
    expected: `${TEST_SEARCH_TERM}/${TEST_SEARCH_TERM}`,
  },
  // Optional parameters are replaced if we known them.
  { param: "{searchTerms?}", expected: TEST_SEARCH_TERM },
  { param: "{unknownOptional?}", expected: "" },
  { param: "{unknownRequired}", expected: "{unknownRequired}" },

  { param: "{language}", expected: Services.locale.requestedLocale },
  { param: "{language?}", expected: Services.locale.requestedLocale },

  { param: "{inputEncoding}", charset: "UTF-8", expected: "UTF-8" },
  {
    param: "{inputEncoding?}",
    charset: "windows-1252",
    expected: "windows-1252",
  },
  // Output encoding is always UTF-8 regardless of the input encoding.
  {
    param: "{outputEncoding}",
    charset: "windows-1252",
    expected: "UTF-8",
  },
  { param: "{outputEncoding?}", charset: "UTF-8", expected: "UTF-8" },
  { param: "{count}", expected: "20" },
  { param: "{count?}", expected: "" },
  { param: "{startIndex}", expected: "1" },
  { param: "{startIndex?}", expected: "" },
  { param: "{startPage}", expected: "1" },
  { param: "{startPage?}", expected: "" },
];

add_setup(async function () {
  let id = 0;
  let config = TESTS.map(t => {
    return {
      identifier: `engine-${id++}`,
      base: {
        charset: t.charset ?? "UTF-8",
        urls: {
          search: {
            base: "https://example.com",
            params: [
              {
                name: "sourceId",
                value: "ncc-1701-c",
              },
              {
                name: "test",
                value: t.param,
              },
            ],
            searchTermParamName: "search",
          },
        },
      },
    };
  });

  SearchTestUtils.setRemoteSettingsConfig(config);
  await Services.search.init();
});

add_task(async function test_paramSubstitution() {
  for (let [i, test] of TESTS.entries()) {
    let engine = await Services.search.getEngineById(`engine-${i}`);
    let submission = engine.getSubmission(TEST_SEARCH_TERM);

    Assert.equal(
      submission.uri.spec,
      `https://example.com/?sourceId=ncc-1701-c&test=${test.expected}&search=${TEST_SEARCH_TERM}`,
      `Should have the expected replacement for ${test.param}`
    );
  }
});
