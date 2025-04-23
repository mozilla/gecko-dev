/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

const CLEANUP_UNKNOWN = 0;
const CLEANUP_COMPLETED = 1;
const CLEANUP_COMPLETED_WITH_BUILTIN = 2;

// Cleanup any colorway builtin theme that may still be installed.
async function uninstallAllColorwayBuiltinThemes(activeThemeID) {
  const CLEANUP_PREF = "extensions.colorway-builtin-themes-cleanup";

  if (
    Services.prefs.getIntPref(CLEANUP_PREF, CLEANUP_UNKNOWN) != CLEANUP_UNKNOWN
  ) {
    return false;
  }

  let builtinColorwayThemeFound = false;
  let activeThemeUninstalling = false;

  const themes = await lazy.AddonManager.getAddonsByTypes(["theme"]);
  for (const theme of themes) {
    if (theme.isBuiltinColorwayTheme) {
      builtinColorwayThemeFound = true;
      if (theme.id === activeThemeID) {
        activeThemeUninstalling = true;
      }

      theme.uninstall();
    }
  }

  Services.prefs.setIntPref(
    CLEANUP_PREF,
    builtinColorwayThemeFound
      ? CLEANUP_COMPLETED_WITH_BUILTIN
      : CLEANUP_COMPLETED
  );

  return activeThemeUninstalling;
}

export const ColorwayThemeMigration = {
  maybeWarn: async () => {
    const activeThemeID = Services.prefs.getCharPref(
      "extensions.activeThemeID",
      ""
    );

    // Let's remove all the existing colorwy builtin themes.
    const activeThemeUninstalled = await uninstallAllColorwayBuiltinThemes(
      activeThemeID
    ).catch(err => {
      console.warn("Error on uninstalling all colorways builtin themes", err);
    });

    if (!activeThemeUninstalled) {
      return;
    }

    // This can go async.
    lazy.AddonManager.getAddonByID(lazy.AddonSettings.DEFAULT_THEME_ID).then(
      addon => addon.enable()
    );

    const win = lazy.BrowserWindowTracker.getTopWindow();
    win.MozXULElement.insertFTLIfNeeded("toolkit/global/extensions.ftl");

    win.gNotificationBox.appendNotification(
      "colorway-theme-migration",
      {
        label: {
          "l10n-id": "webext-colorway-theme-migration-notification-message",
        },
        priority: win.gNotificationBox.PRIORITY_INFO_MEDIUM,
      },
      [
        {
          supportPage: "colorways",
        },
        {
          "l10n-id": "webext-colorway-theme-migration-notification-button",
          callback: () => {
            win.openTrustedLinkIn(
              "https://addons.mozilla.org/firefox/collections/4757633/colorways/",
              "tab"
            );
          },
        },
      ]
    );
  },
};
