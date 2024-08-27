/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const prefix = "https://www.example.com/search?q=";

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([
    { identifier: "utf8", base: { charset: "UTF-8" } },
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

add_task(async function test_getSubmission_utf8() {
  let engine = Services.search.getEngineById("utf8");
  testEncode(engine, "UTF-8", "caff\u00E8", "caff%C3%A8");
});

add_task(async function test_getSubmission_windows1252() {
  let engine = Services.search.getEngineById("windows1252");
  testEncode(engine, "windows-1252", "caff\u00E8", "caff%E8");
});
