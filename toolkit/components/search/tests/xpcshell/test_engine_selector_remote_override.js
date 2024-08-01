/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
});

const TEST_CONFIG = [
  {
    base: {
      urls: {
        search: {
          base: "https://www.bing.com/search",
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
    identifier: "aol",
    recordType: "engine",
  },
  {
    recordType: "defaultEngines",
    globalDefault: "aol",
    specificDefaults: [],
  },
  {
    orders: [],
    recordType: "engineOrders",
  },
];

const TEST_CONFIG_OVERRIDE = [
  {
    identifier: "aol",
    urls: {
      search: {
        params: [{ name: "new_param", value: "new_value" }],
      },
    },
    telemetrySuffix: "tsfx",
    clickUrl: "https://aol.url",
  },
];

const engineSelector = new SearchEngineSelector();

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(TEST_CONFIG, TEST_CONFIG_OVERRIDE);
});

add_task(async function test_engine_selector() {
  let { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "us",
  });
  Assert.equal(engines[0].telemetrySuffix, "tsfx");
  Assert.equal(engines[0].clickUrl, "https://aol.url");
  Assert.equal(engines[0].urls.search.params[0].name, "new_param");
  Assert.equal(engines[0].urls.search.params[0].value, "new_value");
});
