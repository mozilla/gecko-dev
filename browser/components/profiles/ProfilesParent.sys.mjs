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
      isDark: false,
      useInAutomation: true,
    },
  ],
  [
    "{b90acfd0-f0fc-4add-9195-f6306d25cdfa}",
    {
      dataL10nId: "profiles-marigold-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381985/marigold-1.9.xpi",
      colors: {
        chromeColor: "#F1CA52",
        toolbarColor: "#FBDF8C",
        contentColor: "#FEF7E0",
      },
      isDark: false,
    },
  ],
  [
    "{388d9fae-8a28-4f9f-9aad-fb9e84e4f3c3}",
    {
      dataL10nId: "profiles-lichen-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381979/lichen_soft-1.3.xpi",
      colors: {
        chromeColor: "#D2E4DA",
        toolbarColor: "#E9F2EC",
        contentColor: "#F5F9F7",
      },
      isDark: false,
    },
  ],
  [
    "{3ac3b0d7-f017-40e1-b142-a26f794e7015}",
    {
      dataL10nId: "profiles-magnolia-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381978/magnolia-1.1.xpi",
      colors: {
        chromeColor: "#FB5B9E",
        toolbarColor: "#F986B6",
        contentColor: "#FBE0ED",
      },
      isDark: false,
    },
  ],
  [
    "{ba48d251-0732-45c2-9f2f-39c68e82d047}",
    {
      dataL10nId: "profiles-lavender-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381983/lavender_soft-1.2.xpi",
      colors: {
        chromeColor: "#CDC1EA",
        toolbarColor: "#EBE4FA",
        contentColor: "#F4F0FD",
      },
      isDark: false,
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
      isDark: true,
      useInAutomation: true,
    },
  ],
  [
    "{750fa518-b61f-4068-9974-330dcf45442f}",
    {
      dataL10nId: "profiles-ocean-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381977/ocean_dark-1.1.xpi",
      colors: {
        chromeColor: "#080D33",
        toolbarColor: "#050D5B",
        contentColor: "#000511",
      },
      isDark: true,
    },
  ],
  [
    "{25b5a343-4238-4bae-b1f9-93a33f258167}",
    {
      dataL10nId: "profiles-terracotta-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381976/terracotta_dark-1.1.xpi",
      colors: {
        chromeColor: "#591305",
        toolbarColor: "#98240B",
        contentColor: "#060100",
      },
      isDark: true,
    },
  ],
  [
    "{f9261f02-c03c-4352-92ee-78dd8b41ca98}",
    {
      dataL10nId: "profiles-moss-theme",
      downloadURL:
        "https://addons.mozilla.org/firefox/downloads/file/4381975/moss_dark-1.1.xpi",
      colors: {
        chromeColor: "#405948",
        toolbarColor: "#5B7B65",
        contentColor: "#323433",
      },
      isDark: true,
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
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
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

        try {
          await SelectableProfileService.deleteCurrentProfile();

          // Finally, exit.
          Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
        } catch (e) {
          // This is expected in tests.
          console.error(e);
        }
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
          .flatMap(win => win.gBrowser.openTabs.length)
          .reduce((total, current) => total + current);
        let loginCount = (await lazy.LoginHelper.getAllUserFacingLogins())
          .length;

        let db = await lazy.PlacesUtils.promiseDBConnection();
        let bookmarksQuery = `SELECT count(*) FROM moz_bookmarks b
                    JOIN moz_bookmarks t ON t.id = b.parent
                    AND t.parent <> :tags_folder
                    WHERE b.type = :type_bookmark`;
        let bookmarksQueryParams = {
          tags_folder: lazy.PlacesUtils.tagsFolderId,
          type_bookmark: lazy.PlacesUtils.bookmarks.TYPE_BOOKMARK,
        };
        let bookmarkCount = (
          await db.executeCached(bookmarksQuery, bookmarksQueryParams)
        )[0].getResultByIndex(0);

        let stats = await lazy.PlacesDBUtils.getEntitiesStatsAndCounts();
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
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        // Where the theme was installed from
        let telemetryInfo = {
          source: gBrowser.selectedBrowser.currentURI.displaySpec,
          method: "url",
        };
        await this.enableTheme(themeId, telemetryInfo);
        // The enable theme promise resolves after the
        // "lightweight-theme-styling-update" observer so we know the profile
        // theme is up to date at this point.
        return SelectableProfileService.currentProfile.theme;
      }
      case "Profiles:CloseProfileTab": {
        let gBrowser = this.browsingContext.topChromeWindow.gBrowser;
        gBrowser.removeTab(this.tab);
        break;
      }
    }
    return null;
  }

  async enableTheme(themeId, telemetryInfo) {
    let theme = await lazy.AddonManager.getAddonByID(themeId);
    if (!theme) {
      let themeUrl = PROFILE_THEMES_MAP.get(themeId).downloadURL;
      let themeInstall = await lazy.AddonManager.getInstallForURL(themeUrl, {
        telemetryInfo,
      });
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
          isDark: themeObj.isDark,
          useInAutomation: themeObj?.useInAutomation,
        });
      } else {
        themes.push({
          id: themeId,
          dataL10nId: themeObj.dataL10nId,
          isActive: false,
          ...themeObj.colors,
          isDark: themeObj.isDark,
          useInAutomation: themeObj?.useInAutomation,
        });
      }
    }

    return themes;
  }
}
