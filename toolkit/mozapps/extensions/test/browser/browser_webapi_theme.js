"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const URL = `${SECURE_TESTROOT}addons/browser_theme.xpi`;

add_task(async function test_theme_install() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    let updates = [];
    function observer(subject) {
      updates.push(JSON.stringify(subject.wrappedJSObject));
    }
    Services.obs.addObserver(observer, "lightweight-theme-styling-update");
    registerCleanupFunction(() => {
      Services.obs.removeObserver(observer, "lightweight-theme-styling-update");
    });

    async function runInstallThemeTest(callback) {
      let sawConfirm = false;
      promisePopupNotificationShown("addon-install-confirmation").then(
        panel => {
          sawConfirm = true;
          panel.button.click();
        }
      );

      let prompt1 = waitAppMenuNotificationShown(
        "theme-installed",
        "theme@tests.mozilla.org",
        false
      );
      let installPromise = SpecialPowers.spawn(browser, [URL], async url => {
        let install = await content.navigator.mozAddonManager.createInstall({
          url,
        });
        return install.install();
      }).then(() => info(`mozAddonManager install promise resolved`));
      await prompt1;

      ok(sawConfirm, "Confirm notification was displayed before installation");

      // Open a new window and test the app menu panel from there.  This verifies the
      // incognito checkbox as well as finishing install in this case.
      let newWin = await BrowserTestUtils.openNewBrowserWindow();
      const popupnotification = await waitAppMenuNotificationShown(
        "theme-installed",
        "theme@tests.mozilla.org",
        false,
        newWin
      );
      // Call the callback and pass the popupnotification to allow the callback
      // to interact with the popupnotification primary and secondary buttons.
      await callback({ popupnotification });
      await installPromise;
      ok(true, "Theme install completed");

      await BrowserTestUtils.closeWindow(newWin);
    }

    info(
      "Trigger theme install and then click the Undo post-install dialog button"
    );

    const initialActiveThemeID = Services.prefs.getCharPref(
      "extensions.activeThemeID"
    );
    await runInstallThemeTest(async ({ popupnotification }) => {
      Assert.equal(
        Services.prefs.getCharPref("extensions.activeThemeID"),
        "theme@tests.mozilla.org",
        "The newly installed theme is the current active theme"
      );
      const promisePreviousThemeEnabled = AddonTestUtils.promiseAddonEvent(
        "onEnabled",
        initialActiveThemeID
      );
      ok(
        popupnotification.secondaryButton,
        "Found a secondary button in the post install dialog"
      );
      info("Click Undo button in the theme post install dialog");
      popupnotification.secondaryButton.click();
      info(
        "Wait to active theme to be reverted to the previously active theme"
      );
      await promisePreviousThemeEnabled;
      Assert.equal(
        Services.prefs.getCharPref("extensions.activeThemeID"),
        initialActiveThemeID,
        "The active theme id is set to the previously active theme"
      );
    });

    // Expect 2 updates: one for the theme install and one for the theme undo.
    Assert.equal(updates.length, 2, "Got expected number of theme updates");
    updates = [];

    info(
      "Trigger theme install again and then click the Ok post-install dialog button"
    );
    await runInstallThemeTest(async ({ popupnotification }) => {
      ok(
        popupnotification.button,
        "Found primary button in the post install dialog"
      );
      info("Click Ok button in the theme post install dialog");
      popupnotification.button.click();
    });

    Assert.equal(updates.length, 1, "Got expected number of theme updates");
    let parsed = JSON.parse(updates[0]);
    ok(
      parsed.theme.headerURL.endsWith("/testImage.png"),
      "Theme update has the expected headerURL"
    );
    is(
      parsed.theme.id,
      "theme@tests.mozilla.org",
      "Theme update includes the theme ID"
    );
    is(
      parsed.theme.version,
      "1.0",
      "Theme update includes the theme's version"
    );

    let addon = await AddonManager.getAddonByID(parsed.theme.id);
    await addon.uninstall();
  });
});
