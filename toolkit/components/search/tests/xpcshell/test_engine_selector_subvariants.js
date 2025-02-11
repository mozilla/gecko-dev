/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const STATIC_ENGINE_INFO = {
  identifier: "engine-1",
  name: "engine-1",
  classification: "general",
  urls: {
    search: {
      base: "https://www.example.com/search",
      searchTermParamName: "q",
    },
  },
};

const CONFIG = [
  {
    ...STATIC_ENGINE_INFO,
    variants: [
      {
        environment: {
          allRegionsAndLocales: true,
        },
        partnerCode: "variant-partner-code",
        subVariants: [
          {
            environment: { regions: ["CA", "FR"] },
            telemetrySuffix: "subvariant-telemetry",
          },
          {
            environment: { regions: ["GB", "FR"] },
            partnerCode: "subvariant-partner-code",
          },
        ],
      },
    ],
  },
];

const engineSelector = new SearchEngineSelector();

add_task(async function test_no_subvariants_match() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "fi",
      region: "FI",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "variant-partner-code",
        ...STATIC_ENGINE_INFO,
      },
    ],
    "Should match no subvariants."
  );
});

add_task(async function test_matching_subvariant_with_properties() {
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
        partnerCode: "subvariant-partner-code",
        ...STATIC_ENGINE_INFO,
      },
    ],
    "Should match subvariant with subvariant properties."
  );
});

add_task(async function test_matching_variant_and_subvariant_with_properties() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "en-CA",
      region: "CA",
    },
    [
      {
        partnerCode: "variant-partner-code",
        telemetrySuffix: "subvariant-telemetry",
        ...STATIC_ENGINE_INFO,
      },
    ],
    "Should match subvariant with subvariant properties."
  );
});

add_task(async function test_matching_two_subvariant_with_properties() {
  await assertSelectorEnginesEqualsExpected(
    engineSelector,
    CONFIG,
    {
      locale: "fr",
      region: "FR",
    },
    [
      {
        identifier: "engine-1",
        partnerCode: "subvariant-partner-code",
        ...STATIC_ENGINE_INFO,
      },
    ],
    "Should match the last subvariant with subvariant properties."
  );
});
