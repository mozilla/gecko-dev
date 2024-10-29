/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfileService } from "resource:///modules/profiles/SelectableProfileService.sys.mjs";

const lazy = {};

// Bug 1922374: Move themes to remote settings
const PROFILE_THEMES_MAP = new Map([
  [
    "firefox-compact-light@mozilla.org",
    {
      colors: {
        chromeColor: "#F0F0F4",
        toolbarColor: "#F9F9FB",
        contentColor: "#FFFFFF",
      },
    },
  ],
  [
    "expressionist-soft-colorway@mozilla.org",
    {
      name: "Expressionist – Soft",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066185/expressionist_soft-2.1.xpi",
      colors: {
        chromeColor: "#F1CA52",
        toolbarColor: "#FBDF8C",
        contentColor: "#FEF7E0",
      },
    },
  ],
  [
    "lush-soft-colorway@mozilla.org",
    {
      name: "Lush – Soft",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066281/lush_soft-2.1.xpi",
      colors: {
        chromeColor: "#D2E4DA",
        toolbarColor: "#E9F2EC",
        contentColor: "#F5F9F7",
      },
    },
  ],
  [
    "playmaker-soft-colorway@mozilla.org",
    {
      name: "Playmaker – Soft",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066243/playmaker_soft-2.1.xpi",
      colors: {
        chromeColor: "#FB5B9E",
        toolbarColor: "#F986B6",
        contentColor: "#FBE0ED",
      },
    },
  ],
  [
    "dreamer-soft-colorway@mozilla.org",
    {
      name: "Dreamer – Soft",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066182/dreamer_soft-2.1.xpi",
      colors: {
        chromeColor: "#CDC1EA",
        toolbarColor: "#EBE4FA",
        contentColor: "#F4F0FD",
      },
    },
  ],
  [
    "firefox-compact-dark@mozilla.org",
    {
      colors: {
        chromeColor: "#1C1B22",
        toolbarColor: "#2B2A33",
        contentColor: "#42414D",
      },
    },
  ],
  [
    "activist-bold-colorway@mozilla.org",
    {
      name: "Activist – Bold",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066178/activist_bold-2.1.xpi",
      colors: {
        chromeColor: "#080D33",
        toolbarColor: "#050D5B",
        contentColor: "#000511",
      },
    },
  ],
  [
    "playmaker-bold-colorway@mozilla.org",
    {
      name: "Playmaker – Bold",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066242/playmaker_bold-2.1.xpi",
      colors: {
        chromeColor: "#591305",
        toolbarColor: "#98240B",
        contentColor: "#060100",
      },
    },
  ],
  [
    "elemental-bold-colorway@mozilla.org",
    {
      name: "Elemental – Bold",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4066261/elemental_bold-2.1.xpi",
      colors: {
        chromeColor: "#405948",
        toolbarColor: "#5B7B65",
        contentColor: "#323433",
      },
    },
  ],
  [
    "default-theme@mozilla.org",
    {
      colors: {
        chromeColor: "#1C1B22",
        toolbarColor: "#2B2A33",
        contentColor: "#42414D",
      },
    },
  ],
]);

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  formAutofillStorage: "resource://autofill/FormAutofillStorage.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

export class ProfilesParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "Profiles:DeleteProfile": {
        // TODO (bug 1918523): update default and handle deletion in a
        //                     background task.
        break;
      }
      case "Profiles:CancelDelete": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.removeTab(gBrowser.selectedTab);
        break;
      }
      // Intentional fallthrough
      case "Profiles:GetNewProfileContent":
      case "Profiles:GetEditProfileContent": {
        // Make sure SelectableProfileService is initialized
        await SelectableProfileService.init();
        let currentProfile = SelectableProfileService.currentProfile;
        let profiles = await SelectableProfileService.getAllProfiles();
        let themes = await this.getSafeForContentThemes();
        return {
          currentProfile: currentProfile.toObject(),
          profiles: profiles.map(p => p.toObject()),
          themes,
          isInAutomation: Cu.isInAutomation,
        };
      }
      case "Profiles:OpenDeletePage": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.selectedBrowser.loadURI(
          Services.io.newURI("about:deleteprofile"),
          {
            triggeringPrincipal:
              Services.scriptSecurityManager.getSystemPrincipal(),
          }
        );
        break;
      }
      case "Profiles:UpdateProfileName": {
        let profileObj = message.data;
        SelectableProfileService.currentProfile.name = profileObj.name;
        break;
      }
      case "Profiles:GetDeleteProfileContent": {
        let profileObj = SelectableProfileService.currentProfile.toObject();
        let windowCount = lazy.EveryWindow.readyWindows.length;
        let tabCount = lazy.EveryWindow.readyWindows
          .flatMap(win => win.gBrowser.openTabCount)
          .reduce((total, current) => total + current);
        let loginCount = (await lazy.LoginHelper.getAllUserFacingLogins())
          .length;

        let stats = await lazy.PlacesDBUtils.getEntitiesStatsAndCounts();
        let bookmarkCount = stats.find(
          item => item.entity == "moz_bookmarks"
        ).count;
        let visitCount = stats.find(
          item => item.entity == "moz_historyvisits"
        ).count;
        let cookieCount = Services.cookies.cookies.length;
        let historyCount = visitCount + cookieCount;

        await lazy.formAutofillStorage.initialize();
        let autofillCount =
          lazy.formAutofillStorage.addresses._data.length +
          lazy.formAutofillStorage.creditCards?._data.length;

        return {
          profile: profileObj,
          windowCount,
          tabCount,
          bookmarkCount,
          historyCount,
          autofillCount,
          loginCount,
        };
      }
      case "Profiles:UpdateProfileAvatar": {
        let profileObj = message.data;
        SelectableProfileService.currentProfile.avatar = profileObj.avatar;
        break;
      }
      case "Profiles:UpdateProfileTheme": {
        let themeId = message.data;
        await this.enableTheme(themeId);
        // The enable theme promise resolves after the
        // "lightweight-theme-styling-update" observer so we know the profile
        // theme is up to date at this point.
        return SelectableProfileService.currentProfile.theme;
      }
      case "Profiles:DeleteNewProfile": {
        // TODO: Bug 1925096 actually delete the newly created profile.
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
        break;
      }
      case "Profiles:CloseNewProfileTab": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.removeTab(gBrowser.selectedTab);
        break;
      }
    }
    return null;
  }

  async enableTheme(themeId) {
    let theme = await lazy.AddonManager.getAddonByID(themeId);
    if (!theme) {
      let themeUrl = PROFILE_THEMES_MAP.get(themeId).downloadURL;
      let themeInstall = await lazy.AddonManager.getInstallForURL(themeUrl);
      await themeInstall.install();
      theme = await lazy.AddonManager.getAddonByID(themeId);
    }

    await theme.enable();
  }

  async getSafeForContentThemes() {
    let themes = [];
    for (let [themeId, themeObj] of PROFILE_THEMES_MAP) {
      let theme = await lazy.AddonManager.getAddonByID(themeId);
      if (theme) {
        themes.push({
          id: themeId,
          name: theme.name,
          isActive: theme.isActive,
          ...themeObj.colors,
        });
      } else {
        themes.push({
          id: themeId,
          name: themeObj.name,
          isActive: false,
          ...themeObj.colors,
        });
      }
    }

    return themes;
  }
}
