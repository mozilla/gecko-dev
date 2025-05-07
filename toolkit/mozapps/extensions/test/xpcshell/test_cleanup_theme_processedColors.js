/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const THEME_ID = "theme@tests.mozilla.org";

const { JSONFile } = ChromeUtils.importESModule(
  "resource://gre/modules/JSONFile.sys.mjs"
);

Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION
);

add_task(async function test_cleanup_theme_processedColors() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  const profileDir = gProfD.clone();
  profileDir.append("extensions");

  await promiseWriteWebManifestForExtension(
    {
      author: "Some author",
      manifest_version: 2,
      name: "Web Extension Name",
      version: "1.0",
      theme: {},
      browser_specific_settings: {
        gecko: {
          id: THEME_ID,
        },
      },
    },
    profileDir
  );

  await promiseStartupManager();

  const addon = await AddonManager.getAddonByID(THEME_ID);
  Assert.ok(!!addon, "Theme addon should exist");

  await AddonTestUtils.promiseShutdownManager();

  const data = aomStartup.readStartupData();

  const themeEntry = data["app-profile"].addons[THEME_ID];
  themeEntry.startupData = {
    lwtData: {
      theme: {
        _processedColors: 42,
        foo: "bar",
      },
    },
    lwtStyles: {
      _processedColors: 42,
      foo: "bar",
    },
  };

  const jsonFile = new JSONFile({
    path: PathUtils.join(gProfD.path, "addonStartup.json.lz4"),
    compression: "lz4",
  });
  jsonFile.data = data;
  await jsonFile._save();

  await AddonTestUtils.promiseStartupManager();
  await AddonTestUtils.loadAddonsList(true);

  const startupData = aomStartup.readStartupData();
  const themeFromFile = startupData["app-profile"].addons[THEME_ID];
  Assert.ok(themeFromFile.startupData, "We have startupData");
  Assert.equal(
    themeFromFile.startupData.lwtData.theme.foo,
    "bar",
    "The sentinel value is found"
  );
  Assert.ok(
    !("_processedColors" in themeFromFile.startupData.lwtData.theme),
    "No _processedColor property"
  );
  Assert.equal(
    themeFromFile.startupData.lwtStyles.foo,
    "bar",
    "The sentinel value is found"
  );
  Assert.ok(
    !("_processedColors" in themeFromFile.startupData.lwtStyles),
    "No _processedColor property"
  );
});
