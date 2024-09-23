/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfileService } from "resource:///modules/profiles/SelectableProfileService.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  formAutofillStorage: "resource://autofill/FormAutofillStorage.sys.mjs",
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
  PlacesDBUtils: "resource://gre/modules/PlacesDBUtils.sys.mjs",
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
      case "Profiles:GetEditProfileContent": {
        // Make sure SelectableProfileService is initialized
        await SelectableProfileService.init();
        let currentProfile = SelectableProfileService.currentProfile;
        let profiles = await SelectableProfileService.getAllProfiles();
        return {
          currentProfile: currentProfile.toObject(),
          profiles: profiles.map(p => p.toObject()),
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
        let avatar = SelectableProfileService.currentProfile.avatar;
        let name = SelectableProfileService.currentProfile.name;
        let windowCount = lazy.EveryWindow.readyWindows.length;
        let tabCount = lazy.EveryWindow.readyWindows
          .flatMap(win => win.gBrowser.visibleTabs.length)
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
          avatar,
          name,
          windowCount,
          tabCount,
          bookmarkCount,
          historyCount,
          autofillCount,
          loginCount,
        };
      }
    }
    return null;
  }
}
