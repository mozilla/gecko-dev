/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

add_task(async function test_currentThemeFromAMOExistsOnEditPage() {
  await initGroupDatabase();
  let profile = SelectableProfileService.currentProfile;
  Assert.ok(profile, "Should have a profile now");

  const BLUE_THEME_ID = "blue@test.mozilla.org";
  let blueTheme = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "blue theme",
      version: "1.0",
      browser_specific_settings: { gecko: { id: BLUE_THEME_ID } },
      theme: {
        colors: {
          frame: "blue",
        },
      },
    },
  });

  let install = await lazy.AddonManager.getInstallForFile(
    blueTheme,
    "application/x-xpinstall"
  );
  const themeEnabled = AddonTestUtils.promiseAddonEvent(
    "onEnabled",
    addon => addon.id === BLUE_THEME_ID
  );

  install.install().then(themeAddon => themeAddon.enable());

  await themeEnabled;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:editprofile",
    },
    async browser => {
      await SpecialPowers.spawn(browser, [], async () => {
        let editProfileCard =
          content.document.querySelector("edit-profile-card").wrappedJSObject;

        await ContentTaskUtils.waitForCondition(
          () => editProfileCard.initialized,
          "Waiting for edit-profile-card to be initialized"
        );

        await editProfileCard.updateComplete;

        Assert.equal(
          editProfileCard.themes.length,
          11,
          "Should have 11 themes with the currennt theme from AMO"
        );

        Assert.equal(
          editProfileCard.themes.at(-1).id,
          "blue@test.mozilla.org",
          "The last theme should be the blue test theme"
        );
      });
    }
  );

  const blueThemeAddon = await AddonManager.getAddonByID(BLUE_THEME_ID);
  await blueThemeAddon.uninstall();
});
