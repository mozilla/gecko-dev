/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { AppMenuNotifications } = ChromeUtils.importESModule(
  "resource://gre/modules/AppMenuNotifications.sys.mjs"
);

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

function promisePostInstallNotificationShown() {
  // The themes are unsigned, so we need to confirm we're okay with that first
  // This is why we need to accept a prompt, and then continue to the prompt
  // that is interesting to us (the post-install message)
  const id = "theme-installed";

  return new Promise(resolve => {
    function popupshown() {
      let notification = AppMenuNotifications.activeNotification;
      if (!notification) {
        return;
      }
      is(notification.id, id, `${id} notification shown`);
      ok(window.PanelUI.isNotificationPanelOpen, "notification panel open");

      window.PanelUI.notificationPanel.removeEventListener(
        "popupshown",
        popupshown
      );

      let popupnotificationID = window.PanelUI._getPopupId(notification);
      let popupnotification = document.getElementById(popupnotificationID);
      resolve(popupnotification);
    }
    window.PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
  });
}

async function installTheme(id, theme, { userInstalled = true } = {}) {
  let install = await AddonManager.getInstallForFile(
    theme,
    "application/x-xpinstall"
  );

  const themeEnabled = AddonTestUtils.promiseAddonEvent(
    "onEnabled",
    addon => addon.id === id
  );

  if (userInstalled) {
    // Mock a theme installed by a user.
    AddonManager.installAddonFromAOM(
      gBrowser.selectedBrowser,
      document.documentURIObject,
      install
    );
  } else {
    // Mock a theme installed in the background (e.g. when it is being installed in the
    // background by Firefox Sync).
    install.install().then(themeAddon => themeAddon.enable());
  }

  return themeEnabled;
}

async function promisePostInstallShown() {
  const panel = await promisePopupNotificationShown(
    "addon-install-confirmation"
  );

  panel.button.click();

  // Wait for post install pupup

  let postInstallNotification = await promisePostInstallNotificationShown();
  let okayButton = postInstallNotification.querySelector(
    ".popup-notification-primary-button"
  );
  let undoButton = postInstallNotification.querySelector(
    ".popup-notification-secondary-button"
  );

  return [okayButton, undoButton];
}

// Test that clicking the undo button on the post install notification reverts
// the theme
add_task(async function test_undoTheme() {
  const RED_THEME_ID = "red@test.mozilla.org";
  let redTheme = await AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      name: "red theme",
      browser_specific_settings: { gecko: { id: RED_THEME_ID } },
      theme: {
        colors: {
          frame: "red",
        },
      },
    },
  });

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

  const GREEN_THEME_ID = "green@test.mozilla.org";
  let greenTheme = await AddonTestUtils.createTempWebExtensionFile({
    useAddonManager: "permanent",
    manifest: {
      name: "green theme",
      version: "1.0",
      browser_specific_settings: { gecko: { id: GREEN_THEME_ID } },
      theme: {
        colors: {
          frame: "green",
        },
      },
    },
  });

  // Install a new theme
  let promiseThemeEnabled = installTheme(RED_THEME_ID, redTheme);
  const [okayButton] = await promisePostInstallShown();
  okayButton.click();

  // Make sure the theme is fully enabled before checking the prefs
  await promiseThemeEnabled;

  Assert.strictEqual(
    Services.prefs.getCharPref("extensions.activeThemeID"),
    RED_THEME_ID,
    "Expected red theme to be the active theme"
  );
  Assert.strictEqual(
    (await AddonManager.getAddonByID(RED_THEME_ID)).previousActiveThemeID,
    DEFAULT_THEME_ID,
    "Expected previous active theme ID to be the default theme"
  );

  // After first theme is installed, trigger a second install but "undo" it
  promiseThemeEnabled = installTheme(BLUE_THEME_ID, blueTheme);
  const [, undoButton] = await promisePostInstallShown();
  // At this point, the second (blue) theme is installed and we haven't
  // reverted to the prevoius theme yet
  // Make sure the theme is fully enabled before checking the prefs
  await promiseThemeEnabled;
  Assert.strictEqual(
    Services.prefs.getCharPref("extensions.activeThemeID"),
    BLUE_THEME_ID,
    "Expected active theme before undo to be the blue theme"
  );
  Assert.strictEqual(
    (await AddonManager.getAddonByID(BLUE_THEME_ID)).previousActiveThemeID,
    RED_THEME_ID,
    "Expected previous active theme ID before undo to be the red theme"
  );

  info(
    "Install green theme without user interaction (mocks Firefox Sync installed theme)"
  );
  // Simulate a theme installed without using interaction (e.g. like it would
  // be the case if Firefox Sync would be installing a theme as part of syncing
  // the installed add-ons).
  await installTheme(GREEN_THEME_ID, greenTheme, { userInstalled: false });
  Assert.strictEqual(
    Services.prefs.getCharPref("extensions.activeThemeID"),
    GREEN_THEME_ID,
    "Expected new active theme to be the green theme"
  );

  // Expect the previousActiveThemeID collected before to still be set to the
  // theme id that was active when the blue theme was being installed.
  Assert.strictEqual(
    (await AddonManager.getAddonByID(BLUE_THEME_ID)).previousActiveThemeID,
    RED_THEME_ID,
    "Expected previous active theme ID before undo to still be the red theme"
  );

  info("Click theme undo button and expect the red theme to be re-enabled");
  promiseThemeEnabled = AddonTestUtils.promiseAddonEvent(
    "onEnabled",
    addon => addon.id === RED_THEME_ID
  );
  undoButton.click();

  // Make sure the theme is fully enabled before checking the prefs
  await promiseThemeEnabled;
  Assert.strictEqual(
    RED_THEME_ID,
    Services.prefs.getCharPref("extensions.activeThemeID"),
    "Expected active theme after undo to be the red theme"
  );

  const redThemeAddon = await AddonManager.getAddonByID(RED_THEME_ID);
  await redThemeAddon.uninstall();
});
