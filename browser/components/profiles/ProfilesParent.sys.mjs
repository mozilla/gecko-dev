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
      dataL10nId: "profiles-light-theme",
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
      dataL10nId: "profiles-marigold-theme",
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
      dataL10nId: "profiles-lichen-theme",
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
      dataL10nId: "profiles-magnolia-theme",
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
      dataL10nId: "profiles-lavender-theme",
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
      dataL10nId: "profiles-dark-theme",
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
      dataL10nId: "profiles-ocean-theme",
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
      dataL10nId: "profiles-terracotta-theme",
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
      dataL10nId: "profiles-moss-theme",
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
      dataL10nId: "profiles-system-theme",
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
  get tab() {
    const gBrowser = this.browsingContext.topChromeWindow.gBrowser;
    const tab = gBrowser.getTabForBrowser(this.browsingContext.embedderElement);
    return tab;
  }

  actorCreated() {
    let favicon = this.tab.iconImage;
    favicon.classList.add("profiles-tab");
  }

  didDestroy() {
    const gBrowser = this.browsingContext.topChromeWindow?.gBrowser;
    if (!gBrowser) {
      // If gBrowser doesn't exist, then we've closed the tab so we can just return
      return;
    }
    let favicon = this.tab.iconImage;
    favicon.classList.remove("profiles-tab");
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "Profiles:DeleteProfile": {
        let profiles = await SelectableProfileService.getAllProfiles();

        if (profiles.length <= 1) {
          return null;
        }

        // Notify windows that a quit has been requested.
        let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
          Ci.nsISupportsPRBool
        );
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

        if (cancelQuit.data) {
          // Something blocked our attempt to quit.
          return null;
        }

        await SelectableProfileService.deleteCurrentProfile();

        // Finally, exit.
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
        break;
      }
      case "Profiles:CancelDelete": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.removeTab(this.tab);
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
        // Make sure SelectableProfileService is initialized
        await SelectableProfileService.init();
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
      case "Profiles:CloseProfileTab": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.removeTab(this.tab);
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
          dataL10nId: themeObj.dataL10nId,
          isActive: theme.isActive,
          ...themeObj.colors,
        });
      } else {
        themes.push({
          id: themeId,
          dataL10nId: themeObj.dataL10nId,
          isActive: false,
          ...themeObj.colors,
        });
      }
    }

    return themes;
  }
}
