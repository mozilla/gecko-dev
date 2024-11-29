/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that configuration provided engines installed by the user
 * are installed and persisted correctly.
 */

"use strict";

const CONFIG = [
  {
    recordType: "engine",
    identifier: "default",
    base: {
      name: "Default Engine",
      urls: {
        search: {
          base: "https://example.org",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "additional",
    base: {
      name: "Additional Engine",
      urls: {
        search: {
          base: "https://example.net",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { locales: ["de"] },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "default",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async function () {
  SearchTestUtils.updateRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async () => {
  let initialEngines = await Services.search.getVisibleEngines();
  Assert.ok(initialEngines.length, "There are initial engines installed");

  let engine =
    await Services.search.findContextualSearchEngineByHost("example.net");
  let settingsFileWritten = promiseAfterSettings();
  await Services.search.addSearchEngine(engine);
  await settingsFileWritten;

  let newEngines = await Services.search.getVisibleEngines();
  Assert.ok(
    newEngines.length > initialEngines.length,
    "New engine is installed"
  );

  let updatedName = "Updated Additional Engine";
  CONFIG[1].base.name = updatedName;
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG);

  Assert.ok(
    (await Services.search.getVisibleEngines()).length > initialEngines.length,
    "Engine is persisted after reload"
  );

  Assert.ok(
    await Services.search.getEngineByName(updatedName),
    "The engines details are updated when configuration changes"
  );

  settingsFileWritten = promiseAfterSettings();
  await Services.search.wrappedJSObject.reset();
  await Services.search.init(true);
  await settingsFileWritten;

  Assert.ok(
    (await Services.search.getVisibleEngines()).length > initialEngines.length,
    "Engine is persisted after restart"
  );
});
