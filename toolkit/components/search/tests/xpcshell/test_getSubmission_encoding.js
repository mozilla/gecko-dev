/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefix = "https://www.example.com/search";

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([
    {
      identifier: "utf8_param",
      base: {
        charset: "UTF-8",
        urls: {
          search: {
            base: "https://www.example.com/search",
            searchTermParamName: "q",
          },
        },
      },
    },
    {
      identifier: "utf8_url",
      base: {
        charset: "UTF-8",
        urls: {
          search: {
            base: "https://www.example.com/search/{searchTerms}",
          },
        },
      },
    },
    { identifier: "windows1252", base: { charset: "windows-1252" } },
  ]);
  await Services.search.init();
});

function testEncode(engine, charset, query, expected) {
  Assert.equal(
    engine.getSubmission(query).uri.spec,
    prefix + expected,
    `Should have correctly encoded for ${charset}`
  );
}

add_task(async function test_getSubmission_utf8_param() {
  let engine = Services.search.getEngineById("utf8_param");
  // Space should be encoded to + since the search terms are a parameter.
  testEncode(engine, "UTF-8", "caff\u00E8 shop+", "?q=caff%C3%A8+shop%2B");
});

add_task(async function test_getSubmission_utf8_url() {
  let engine = Services.search.getEngineById("utf8_url");
  // Space should be encoded to %20 since the search terms are part of the URL.
  testEncode(engine, "UTF-8", "caff\u00E8 shop+", "/caff%C3%A8%20shop%2B");
});

add_task(async function test_getSubmission_windows1252() {
  let engine = Services.search.getEngineById("windows1252");
  testEncode(engine, "windows-1252", "caff\u00E8+", "?q=caff%E8%2B");
});
