/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests searchUrlDomain API.
 */

"use strict";

const CONFIG = [
  {
    identifier: "appDefault",
    base: {
      urls: {
        search: { base: "https://www.example.com", searchTermParamName: "q" },
      },
    },
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

add_task(async function test_resultDomain() {
  await Services.search.init();

  let engine = Services.search.getEngineById("appDefault");

  Assert.equal(engine.searchUrlDomain, "www.example.com");
});
