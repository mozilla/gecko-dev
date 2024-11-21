/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  { identifier: "appDefault" },
  {
    identifier: "non-experiment",
    base: {
      name: "same-name",
      urls: {
        search: {
          base: "https://www.google.com/search",
          searchTermParamName: "q",
        },
      },
    },
  },
  {
    identifier: "experiment",
    base: {
      name: "same-name",
      urls: {
        search: {
          base: "https://www.example.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true, experiment: "xpcshell" },
      },
    ],
  },
];

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

// This is to verify that the loaded configuration matches what we expect for
// the test.
add_task(async function test_initial_config_correct() {
  await Services.search.init();

  const installedEngines = await Services.search.getAppProvidedEngines();
  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    ["appDefault", "non-experiment"],
    "Should have the correct list of engines installed."
  );

  Assert.equal(
    (await Services.search.getDefault()).identifier,
    "appDefault",
    "Should have loaded the expected default engine"
  );
});

add_task(async function test_config_updated_engine_changes() {
  // Update the config.
  const reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
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

  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "experiment",
    "xpcshell"
  );

  await reloadObserved;
  Services.obs.removeObserver(enginesObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  Assert.deepEqual(
    enginesAdded,
    ["experiment"],
    "Should have added the correct engines"
  );

  Assert.deepEqual(
    enginesModified.sort(),
    ["appDefault", "non-experiment"],
    "Should have modified the expected engines"
  );

  Assert.deepEqual(
    enginesRemoved,
    ["non-experiment"],
    "Should have removed the expected engine"
  );

  const installedEngines = await Services.search.getAppProvidedEngines();

  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    ["appDefault", "experiment"],
    "Should have the correct list of engines installed in the expected order."
  );

  const engineWithSameName = await Services.search.getEngineByName("same-name");
  Assert.equal(
    engineWithSameName.getSubmission("test").uri.spec,
    "https://www.example.com/search?q=test",
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
