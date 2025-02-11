/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests the SearchEngineSelector matching the correct variant engine.
 * If multiple variants match, it should add the matching variants to the base
 * cumulatively.
 */

"use strict";

const STATIC_SEARCH_URL_DATA = {
  base: "https://www.example.com/search",
  searchTermParamName: "q",
};

const CONFIG = [
  {
    identifier: "engine-1",
    urls: {
      search: {
        ...STATIC_SEARCH_URL_DATA,
        params: [
          {
            name: "partner-code",
            value: "code",
          },
        ],
      },
    },
    variants: [
      {
        environment: { regions: ["CA", "GB", "IT"] },
        urls: {
          search: {
            params: [],
          },
        },
      },
      {
        environment: { regions: ["CA", "US"] },
        urls: {
          search: {
            params: [
              {
                name: "partner-code",
                value: "bar",
              },
            ],
          },
        },
        telemetrySuffix: "telemetry",
      },
      {
        environment: { regions: ["CA", "GB"] },
        urls: {
          search: {
            params: [
              {
                name: "partner-code",
                value: "foo",
              },
            ],
            searchTermParamName: "search-param",
          },
        },
      },
    ],
  },
];

const CONFIG_CLONE = structuredClone(CONFIG);

const engineSelector = new SearchEngineSelector();

add_task(async function test_no_variants_match() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "fi",
      region: "FI",
    },
    [],
    "Should match no variants."
  );
});

add_task(async function test_match_and_apply_last_variants() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "en-US",
      region: "CA",
    },
    [
      {
        identifier: "engine-1",
        name: "engine-1",
        classification: "general",
        urls: {
          search: {
            ...STATIC_SEARCH_URL_DATA,
            params: [{ name: "partner-code", value: "foo" }],
            searchTermParamName: "search-param",
          },
        },
      },
    ],
    "Should match and apply last variant."
  );
});

add_task(async function test_match_middle_variant() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "en-US",
      region: "US",
    },
    [
      {
        identifier: "engine-1",
        name: "engine-1",
        classification: "general",
        urls: {
          search: {
            ...STATIC_SEARCH_URL_DATA,
            params: [{ name: "partner-code", value: "bar" }],
          },
        },
        telemetrySuffix: "telemetry",
      },
    ],
    "Should match first and second variants."
  );
});

add_task(async function test_match_first_and_last_variant() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "en-GB",
      region: "GB",
    },
    [
      {
        identifier: "engine-1",
        name: "engine-1",
        classification: "general",
        urls: {
          search: {
            ...STATIC_SEARCH_URL_DATA,
            params: [{ name: "partner-code", value: "foo" }],
            searchTermParamName: "search-param",
          },
        },
      },
    ],
    "Should match first and last variant."
  );
});

add_task(async function test_match_variant_with_empty_params() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "it",
      region: "IT",
    },
    [
      {
        identifier: "engine-1",
        name: "engine-1",
        classification: "general",
        urls: {
          search: {
            ...STATIC_SEARCH_URL_DATA,
            params: [],
          },
        },
      },
    ],
    "Should match the first variant with empty params."
  );
});

add_task(async function test_config_has_not_been_modified() {
  Assert.deepEqual(
    CONFIG,
    CONFIG_CLONE,
    "Should not modify the original test config after applying variant engines to the base engine."
  );
});
