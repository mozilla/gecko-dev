/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  {
    identifier: "appDefault",
  },
  {
    identifier: "defaultInFRRegion",
  },
  {
    identifier: "hasParamsInFRRegion",
    base: {
      urls: {
        search: {
          base: "https://www.example.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
      {
        environment: { regions: ["FR"] },
        urls: {
          search: {
            params: [
              {
                name: "c",
                value: "my-test",
              },
            ],
            searchTermParamName: "q1",
          },
        },
      },
    ],
  },
  {
    identifier: "notInFRRegion1",
    variants: [{ environment: { excludedRegions: ["FR"] } }],
  },
  {
    identifier: "onlyInFRRegion",
    variants: [
      {
        environment: { regions: ["FR"] },
      },
    ],
  },
  {
    identifier: "notInFRRegion2",
    variants: [
      {
        environment: { excludedRegions: ["FR"] },
      },
    ],
  },
  {
    identifier: "engineOrderedInFR",
    variants: [
      {
        environment: { regions: ["FR"] },
      },
    ],
  },
  {
    identifier: "engineSameName",
    base: {
      name: "Same Name",
      urls: {
        search: {
          base: "https://www.example.org/same",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { excludedRegions: ["FR"] },
      },
    ],
  },
  {
    identifier: "engineSameNameOther",
    base: {
      name: "Same Name",
      urls: {
        search: {
          base: "https://www.example.com/other",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { regions: ["FR"] },
      },
    ],
  },
  {
    specificDefaults: [
      {
        default: "appDefault",
        defaultPrivate: "appDefault",
        environment: { excludedRegions: ["FR"] },
      },
      {
        default: "defaultInFRRegion",
        defaultPrivate: "defaultInFRRegion",
        environment: { regions: ["FR"] },
      },
    ],
  },
  {
    orders: [
      {
        order: ["engineOrderedInFR"],
        environment: { regions: ["FR"] },
      },
    ],
  },
];

async function visibleEngines() {
  return (await Services.search.getVisibleEngines()).map(e => e.identifier);
}

add_setup(async function () {
  Services.prefs.setBoolPref("browser.search.separatePrivateDefault", true);
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  SearchTestUtils.useMockIdleService();
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

// This is to verify that the loaded configuration matches what we expect for
// the test.
add_task(async function test_initial_config_correct() {
  Region._setHomeRegion("", false);

  await Services.search.init();

  const installedEngines = await Services.search.getAppProvidedEngines();
  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    [
      "appDefault",
      "defaultInFRRegion",
      "hasParamsInFRRegion",
      "notInFRRegion1",
      "notInFRRegion2",
      "engineSameName",
    ],
    "Should have the correct list of engines installed."
  );

  Assert.equal(
    (await Services.search.getDefault()).identifier,
    "appDefault",
    "Should have loaded the expected default engine"
  );

  Assert.equal(
    (await Services.search.getDefaultPrivate()).identifier,
    "appDefault",
    "Should have loaded the expected private default engine"
  );
});

add_task(async function test_config_updated_engine_changes() {
  // Update the config.
  const reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  const defaultEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  const defaultPrivateEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  const enginesAdded = [];
  const enginesModified = [];
  const enginesRemoved = [];

  function enginesObs(subject, topic, data) {
    if (data == SearchUtils.MODIFIED_TYPE.ADDED) {
      enginesAdded.push(subject.QueryInterface(Ci.nsISearchEngine).identifier);
    } else if (data == SearchUtils.MODIFIED_TYPE.CHANGED) {
      enginesModified.push(
        subject.QueryInterface(Ci.nsISearchEngine).identifier
      );
    } else if (data == SearchUtils.MODIFIED_TYPE.REMOVED) {
      enginesRemoved.push(
        subject.QueryInterface(Ci.nsISearchEngine).identifier
      );
    }
  }
  Services.obs.addObserver(enginesObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  Region._setHomeRegion("FR", false);

  await Services.search.wrappedJSObject._maybeReloadEngines();

  await reloadObserved;
  Services.obs.removeObserver(enginesObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  Assert.deepEqual(
    enginesAdded,
    ["engineOrderedInFR", "onlyInFRRegion", "engineSameNameOther"],
    "Should have added the correct engines"
  );

  Assert.deepEqual(
    enginesModified.sort(),
    ["hasParamsInFRRegion"],
    "Should have modified the expected engines"
  );

  Assert.deepEqual(
    enginesRemoved,
    ["notInFRRegion1", "notInFRRegion2", "engineSameName"],
    "Should have removed the expected engines"
  );

  const installedEngines = await Services.search.getAppProvidedEngines();

  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    [
      "defaultInFRRegion",
      "engineOrderedInFR",
      "appDefault",
      "hasParamsInFRRegion",
      "onlyInFRRegion",
      "engineSameNameOther",
    ],
    "Should have the correct list of engines installed in the expected order."
  );

  const newDefault = await defaultEngineChanged;
  Assert.equal(
    newDefault.QueryInterface(Ci.nsISearchEngine).name,
    "defaultInFRRegion",
    "Should have correctly notified the new default engine"
  );

  const newDefaultPrivate = await defaultPrivateEngineChanged;
  Assert.equal(
    newDefaultPrivate.QueryInterface(Ci.nsISearchEngine).name,
    "defaultInFRRegion",
    "Should have correctly notified the new default private engine"
  );

  const engineWithParams = await Services.search.getEngineById(
    "hasParamsInFRRegion"
  );
  Assert.equal(
    engineWithParams.getSubmission("test").uri.spec,
    "https://www.example.com/search?c=my-test&q1=test",
    "Should have updated the parameters"
  );

  const engineWithSameName = await Services.search.getEngineById(
    "engineSameNameOther"
  );
  Assert.equal(
    engineWithSameName.getSubmission("test").uri.spec,
    "https://www.example.com/other?q=test",
    "Should have correctly switched to the engine of the same name"
  );

  Assert.equal(
    Services.search.wrappedJSObject._settings.getMetaDataAttribute(
      "useSavedOrder"
    ),
    false,
    "Should not have set the useSavedOrder preference"
  );
});

add_task(async function test_user_settings_persist() {
  let reload = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("");
  await reload;

  Assert.ok(
    (await visibleEngines()).includes("notInFRRegion1"),
    "Rel Searchform engine should be included by default"
  );

  let settingsFileWritten = promiseAfterSettings();
  let engine = await Services.search.getEngineById("notInFRRegion1");
  await Services.search.removeEngine(engine);
  await settingsFileWritten;

  Assert.ok(
    !(await visibleEngines()).includes("notInFRRegion1"),
    "Rel Searchform engine has been removed"
  );

  reload = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("FR");
  await reload;

  reload = SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("");
  await reload;

  Assert.ok(
    !(await visibleEngines()).includes("notInFRRegion1"),
    "Rel Searchform removal should be remembered"
  );
});
