/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading engines when changing the in-use locale of a WebExtension,
 * where the name of the engine changes as well.
 */

"use strict";

const CONFIG = [
  { identifier: "appDefault" },
  {
    identifier: "notGDLocale",
    base: {
      name: "Not GD Locale",
      urls: {
        search: {
          base: "https://en.wikipedia.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [{ environment: { excludedLocales: ["gd"] } }],
  },
  {
    identifier: "localeGD",
    base: {
      name: "GD Locale",
      urls: {
        search: {
          base: "https://gd.wikipedia.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [{ environment: { locales: ["gd"] } }],
  },
];

add_setup(async () => {
  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "en",
    "gd",
  ];
  Services.locale.requestedLocales = ["gd"];

  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async function test_config_updated_engine_changes() {
  let engines = await Services.search.getEngines();
  Assert.deepEqual(
    engines.map(e => e.identifier),
    ["appDefault", "localeGD"],
    "Should have the correct engines installed"
  );

  let engine = await Services.search.getEngineByName("GD Locale");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://gd.wikipedia.com/search?q=test",
    "Should have the gd search url"
  );

  await promiseSetLocale("en");

  engines = await Services.search.getEngines();
  Assert.deepEqual(
    engines.map(e => e.identifier),
    ["appDefault", "notGDLocale"],
    "Should have the correct engines installed after locale change"
  );

  engine = await Services.search.getEngineByName("Not GD Locale");
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    "https://en.wikipedia.com/search?q=test",
    "Should have the en search url"
  );
});
