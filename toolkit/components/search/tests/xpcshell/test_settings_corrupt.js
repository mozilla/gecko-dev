/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  consoleAllowList.push("get: Settings file empty or corrupt.");
  await IOUtils.writeJSON(
    PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME),
    "{invalid json",
    { compress: true }
  );
});

add_task(async function test_settings_corrupt() {
  Assert.equal(
    Services.prefs.getIntPref(
      SearchUtils.BROWSER_SEARCH_PREF + "lastSettingsCorruptTime",
      false
    ),
    0,
    "lastSettingsCorruptTime is initially 0."
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
  Assert.ok(
    lastSettingsCorruptTime > fiveMinAgo,
    "lastSettingsCorruptTime is set to the current time."
  );
});
