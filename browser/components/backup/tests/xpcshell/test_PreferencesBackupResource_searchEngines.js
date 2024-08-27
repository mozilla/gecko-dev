/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PreferencesBackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/PreferencesBackupResource.sys.mjs"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);
const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const SEARCH_PREFS_FILENAME = "search.json.mozlz4";

SearchTestUtils.init(this);

updateAppInfo({ name: "XPCShell", version: "48", platformVersion: "48" });
do_get_profile();

const FAKE_SEARCH_EXTENSION_NAME = "Some WebExtension Search Engine";

add_setup(async function () {
  await SearchTestUtils.setRemoteSettingsConfig([
    { identifier: "engine1" },
    { identifier: "engine2" },
  ]);
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  await Services.search.init();

  await SearchTestUtils.installSearchExtension(
    {
      name: FAKE_SEARCH_EXTENSION_NAME,
      search_url: "https://example.com/plain",
    },
    { setAsDefault: true }
  );

  await SearchTestUtils.promiseSearchNotification(
    "write-settings-to-disk-complete"
  );
});

/**
 * Tests that PreferencesBackupResource will correctly recompute search engine
 * verification hashes after a recovery. This is more of an integration test
 * than what already exists in test_PreferencesBackupResource.js.
 */
add_task(async function test_recover_searchEngines_verified() {
  let preferencesBackupResource = new PreferencesBackupResource();
  let recoveryPath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-recovery-test"
  );
  let destProfilePath = await IOUtils.createUniqueDirectory(
    PathUtils.tempDir,
    "PreferencesBackupResource-test-profile"
  );

  await IOUtils.copy(
    PathUtils.join(PathUtils.profileDir, SEARCH_PREFS_FILENAME),
    PathUtils.join(recoveryPath, SEARCH_PREFS_FILENAME)
  );

  let postRecoveryEntry = await preferencesBackupResource.recover(
    null /* manifestEntry */,
    recoveryPath,
    destProfilePath
  );

  Assert.equal(
    postRecoveryEntry,
    null,
    "PreferencesBackupResource.recover should return null as its post recovery entry"
  );

  let originalSearchEngineSettings = await IOUtils.readJSON(
    PathUtils.join(PathUtils.profileDir, SEARCH_PREFS_FILENAME),
    { decompress: true }
  );

  let recoveredSearchEngineSettings = await IOUtils.readJSON(
    PathUtils.join(destProfilePath, SEARCH_PREFS_FILENAME),
    { decompress: true }
  );

  Assert.notEqual(
    originalSearchEngineSettings.metaData.defaultEngineIdHash,
    recoveredSearchEngineSettings.metaData.defaultEngineIdHash,
    "defaultEngineIdHash was updated."
  );
  Assert.equal(
    recoveredSearchEngineSettings.metaData.defaultEngineIdHash,
    SearchUtils.getVerificationHash(
      originalSearchEngineSettings.metaData.defaultEngineId,
      destProfilePath
    ),
    "defaultEngineIdHash was updated correctly."
  );

  Assert.notEqual(
    originalSearchEngineSettings.metaData.privateDefaultEngineIdHash,
    recoveredSearchEngineSettings.metaData.privateDefaultEngineIdHash,
    "privateDefaultEngineIdHash was updated."
  );
  Assert.equal(
    recoveredSearchEngineSettings.metaData.privateDefaultEngineIdHash,
    SearchUtils.getVerificationHash(
      originalSearchEngineSettings.metaData.privateDefaultEngineId,
      destProfilePath
    ),
    "privateDefaultEngineIdHash was updated correctly."
  );

  Assert.equal(
    originalSearchEngineSettings.engines.length,
    recoveredSearchEngineSettings.engines.length,
    "Got the same number of engines"
  );

  for (let i = 0; i < originalSearchEngineSettings.engines.length; ++i) {
    let originalEngine = originalSearchEngineSettings.engines[i];
    let recoveredEngine = recoveredSearchEngineSettings.engines[i];

    if (originalEngine._metaData.loadPathHash) {
      Assert.ok(
        recoveredEngine._metaData.loadPathHash,
        "Recovered engine also has a loadPathHash"
      );
      Assert.notEqual(
        originalEngine._metaData.loadPathHash,
        recoveredEngine._metaData.loadPathHash,
        "loadPathHash was updated."
      );
      Assert.equal(
        recoveredEngine._metaData.loadPathHash,
        SearchUtils.getVerificationHash(
          originalEngine._loadPath,
          destProfilePath
        ),
        "loadPathHash had the expected value."
      );
    } else {
      Assert.deepEqual(
        originalEngine,
        recoveredEngine,
        "Engine was not changed."
      );
    }
  }

  await maybeRemovePath(recoveryPath);
  await maybeRemovePath(destProfilePath);
});
