/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  consoleAllowList.push("get: Settings file empty or corrupt.");
  consoleAllowList.push("get: Migration failed.");
});

add_task(async function test_settings_invalid_json() {
  await IOUtils.writeUTF8(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    "{invalid json",
    { compress: true }
  );

  Assert.equal(
    Services.prefs.getIntPref(
      SearchUtils.BROWSER_SEARCH_PREF + "lastSettingsCorruptTime",
      false
    ),
    0,
    "lastSettingsCorruptTime is initially 0."
  );
  let notificationBoxStub = sinon.stub(
    Services.search.wrappedJSObject,
    "_showSearchSettingsResetNotificationBox"
  );

  info("init search service");
  const initResult = await Services.search.init();

  info("init'd search service");
  Assert.ok(
    Components.isSuccessCode(initResult),
    "Should have successfully created the search service"
  );

  let lastSettingsCorruptTime = Services.prefs.getIntPref(
    SearchUtils.BROWSER_SEARCH_PREF + "lastSettingsCorruptTime",
    false
  );

  let fiveMinAgo = Date.now() / 1000 - 5 * 60;
  Assert.greater(
    lastSettingsCorruptTime,
    fiveMinAgo,
    "lastSettingsCorruptTime is set to the current time."
  );

  let defaultEngineName = Services.search.defaultEngine.name;
  sinon.assert.calledWith(notificationBoxStub, defaultEngineName);
  notificationBoxStub.restore();

  let file = await IOUtils.readUTF8(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME + ".bak"),
    { decompress: true }
  );
  Assert.equal(file, "{invalid json", "File was backed up.");
});

add_task(async function test_settings_migration_fail() {
  Services.search.wrappedJSObject.reset();

  Services.prefs.setIntPref(
    SearchUtils.BROWSER_SEARCH_PREF + "lastSettingsCorruptTime",
    0
  );

  let settingsTemplate = await readJSONFile(
    do_get_file("settings/v7-loadPath-migration.json")
  );

  // Setting this to {} will cause the migration to fail.
  settingsTemplate.engines[7]._loadPath = {};
  await promiseSaveSettingsData(settingsTemplate);

  let notificationBoxStub = sinon.stub(
    Services.search.wrappedJSObject,
    "_showSearchSettingsResetNotificationBox"
  );

  info("init search service");
  const initResult = await Services.search.init();

  info("init'd search service");
  Assert.ok(
    Components.isSuccessCode(initResult),
    "Should have successfully created the search service"
  );

  let lastSettingsCorruptTime = Services.prefs.getIntPref(
    SearchUtils.BROWSER_SEARCH_PREF + "lastSettingsCorruptTime",
    false
  );

  let fiveMinAgo = Date.now() / 1000 - 5 * 60;
  Assert.greater(
    lastSettingsCorruptTime,
    fiveMinAgo,
    "lastSettingsCorruptTime is set to the current time."
  );

  let defaultEngineName = Services.search.defaultEngine.name;
  sinon.assert.calledWith(notificationBoxStub, defaultEngineName);
  notificationBoxStub.restore();
});
