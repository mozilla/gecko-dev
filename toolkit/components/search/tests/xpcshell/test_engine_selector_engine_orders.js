/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests the SearchEngineSelector in ordering the engines correctly based
 * on the user's environment.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchEngineSelector:
    "moz-src:///toolkit/components/search/SearchEngineSelector.sys.mjs",
});

const ENGINE_ORDERS_CONFIG = [
  { identifier: "default-engine" },
  { identifier: "b-engine" },
  { identifier: "a-engine" },
  { identifier: "c-engine" },
  {
    recordType: "engineOrders",
    orders: [
      {
        environment: { distributions: ["distro"] },
        order: ["default-engine", "a-engine", "b-engine", "c-engine"],
      },
      {
        environment: {
          distributions: ["distro"],
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["default-engine", "c-engine", "b-engine", "a-engine"],
      },
      {
        environment: {
          distributions: ["distro-2"],
        },
        order: ["default-engine", "a-engine", "b-engine"],
      },
    ],
  },
];

const STARTS_WITH_WIKI_CONFIG = [
  { identifier: "default-engine" },
  {
    identifier: "wiki-ca",
    variants: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
      },
    ],
  },
  {
    identifier: "wiki-uk",
    variants: [
      {
        environment: {
          locales: ["en-GB"],
          regions: ["GB"],
        },
      },
    ],
  },
  {
    identifier: "engine-1",
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    identifier: "engine-2",
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
      },
    ],
  },
  {
    recordType: "engineOrders",
    orders: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["default-engine", "wiki*", "engine-1", "engine-2"],
      },
      {
        environment: {
          locales: ["en-GB"],
          regions: ["GB"],
        },
        order: ["default-engine", "wiki*", "engine-1", "engine-2"],
      },
    ],
  },
];

const DEFAULTS_CONFIG = [
  // The identifiers are intentionally in a different order to the names, so
  // that we can test that ordering by the name works correctly.
  { identifier: "alpha-engine-3", base: { name: "Golf" } },
  { identifier: "alpha-engine-2", base: { name: "November" } },
  { identifier: "alpha-engine-1", base: { name: "Sierra" } },
  // These two will be ordered by the `orders` record.
  { identifier: "b-engine", base: { name: "Tango" } },
  { identifier: "a-engine", base: { name: "Uniform" } },
  // These two are the default engines, and so will be listed first.
  { identifier: "default-engine" },
  { identifier: "default-private-engine" },
  {
    globalDefault: "default-engine",
    globalDefaultPrivate: "default-private-engine",
  },
  {
    orders: [
      {
        environment: {
          locales: ["en-CA"],
          regions: ["CA"],
        },
        order: ["b-engine", "a-engine"],
      },
    ],
  },
];

const engineSelector = new SearchEngineSelector();

/**
 * This function asserts if the actual engine identifiers returned equals
 * the expected engines.
 *
 * @param {object} config
 *   A mock search config contain engines.
 * @param {object} userEnv
 *   A fake user's environment including locale and region, experiment, etc.
 * @param {Array} expectedEngineOrders
 *   The array of engine identifers in the expected order.
 * @param {string} message
 *   The description of the test.
 */
async function assertActualEnginesEqualsExpected(
  config,
  userEnv,
  expectedEngineOrders,
  message
) {
  engineSelector._configuration = null;
  SearchTestUtils.setRemoteSettingsConfig(config, []);

  let { engines } = await engineSelector.fetchEngineConfiguration(userEnv);
  let actualEngineOrders = engines.map(engine => engine.identifier);

  info(`${message}`);
  Assert.deepEqual(actualEngineOrders, expectedEngineOrders, message);
}

add_task(async function test_selector_match_engine_orders() {
  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "fr",
      region: "FR",
      distroID: "distro",
    },
    ["default-engine", "a-engine", "b-engine", "c-engine"],
    "Should match engine orders with the distro distribution."
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distro",
    },
    ["default-engine", "c-engine", "b-engine", "a-engine"],
    "Should match engine orders with the distro distribution, en-CA locale and CA region."
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
      distroID: "distro-2",
    },
    ["default-engine", "a-engine", "b-engine", "c-engine"],
    "Should order the first two engines correctly for distro-2 distribution"
  );

  await assertActualEnginesEqualsExpected(
    ENGINE_ORDERS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["default-engine", "a-engine", "b-engine", "c-engine"],
    "Should be sorted in alphabetical order when there's no matching environments."
  );
});

add_task(async function test_selector_match_engine_orders_starts_with() {
  await assertActualEnginesEqualsExpected(
    STARTS_WITH_WIKI_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    ["default-engine", "wiki-ca", "engine-1", "engine-2"],
    "Should list the wiki-ca engine and other engines in correct orders with the en-CA and CA locale region environment."
  );

  await assertActualEnginesEqualsExpected(
    STARTS_WITH_WIKI_CONFIG,
    {
      locale: "en-GB",
      region: "GB",
    },
    ["default-engine", "wiki-uk", "engine-1", "engine-2"],
    "Should list the wiki-ca engine and other engines in correct orders with the en-CA and CA locale region environment."
  );
});

add_task(async function test_selector_match_engine_orders_with_defaults() {
  await assertActualEnginesEqualsExpected(
    DEFAULTS_CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    [
      "default-engine",
      "default-private-engine",
      "b-engine",
      "a-engine",
      "alpha-engine-3",
      "alpha-engine-2",
      "alpha-engine-1",
    ],
    "Should order the default engine first, default private engine second, ordered engines and then the rest in alphabetical order by name."
  );
});
