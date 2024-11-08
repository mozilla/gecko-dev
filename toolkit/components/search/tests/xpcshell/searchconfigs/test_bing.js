/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const test = new SearchConfigTest({
  identifier: "bing",
  aliases: ["@bing"],
  default: {
    // Not included anywhere.
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
  details: [
    {
      included: [{}],
      domain: "bing.com",
      telemetryId:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "bing-esr" : "bing",
      searchUrlCode:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "pc=MOZR" : "pc=MOZI",
    },
  ],
});

add_setup(async function () {
  await test.setup();
});

add_task(async function test_searchConfig_bing() {
  await test.run();
});
