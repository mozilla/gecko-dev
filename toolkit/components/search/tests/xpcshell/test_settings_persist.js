/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the removal of an engine is persisted in search settings.
 */

"use strict";

const CONF_WITH_TEMP = [
  { identifier: "permanent_engine" },
  { identifier: "temp_engine" },
];

const CONF_WITHOUT_TEMP = [{ identifier: "permanent_engine" }];

async function startup() {
  let settingsFileWritten = promiseAfterSettings();
  let ss = new SearchService();
  await ss.init(false);
  await settingsFileWritten;
  return ss;
}

async function visibleEngines(ss) {
  return (await ss.getVisibleEngines()).map(e => e._name);
}

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig(CONF_WITH_TEMP);
  // This is only needed as otherwise events will not be properly notified
  // due to https://searchfox.org/mozilla-central/rev/5f0a7ca8968ac5cef8846e1d970ef178b8b76dcc/toolkit/components/search/SearchSettings.sys.mjs#41-42
  let settingsFileWritten = promiseAfterSettings();
  await Services.search.init(false);
  Services.search.wrappedJSObject._removeObservers();
  await settingsFileWritten;
});

add_task(async function () {
  let ss = await startup();
  Assert.ok(
    (await visibleEngines(ss)).includes("temp_engine"),
    "Should have both engines on first startup"
  );

  let settingsFileWritten = promiseAfterSettings();
  let engine = await ss.getEngineByName("temp_engine");
  await ss.removeEngine(engine);
  await settingsFileWritten;

  Assert.ok(
    !(await visibleEngines(ss)).includes("temp_engine"),
    "temp_engine has been removed, only permanent_engine should remain"
  );

  ss._removeObservers();
  SearchTestUtils.setRemoteSettingsConfig(CONF_WITHOUT_TEMP);
  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("temp_engine"),
    "Updated to new configuration that doesnt have temp_engine"
  );

  ss._removeObservers();
  SearchTestUtils.setRemoteSettingsConfig(CONF_WITH_TEMP);

  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("temp_engine"),
    "Configuration now includes temp_engine but we should remember its removal"
  );
});
