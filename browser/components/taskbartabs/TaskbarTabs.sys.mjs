/* vim: se cin sw=2 ts=2 et filetype=javascript :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kWidgetId = "taskbar-tabs-button";
const kEnabledPref = "browser.taskbarTabs.enabled";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const kWebAppWindowFeatures =
  "chrome,dialog=no,titlebar,close,toolbar,location,personalbar=no,status,menubar=no,resizable,minimizable,scrollbars";

let lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

export let TaskbarTabs = {
  async init(window) {
    if (
      AppConstants.platform != "win" ||
      !Services.prefs.getBoolPref(kEnabledPref, false) ||
      window.document.documentElement.hasAttribute("taskbartab") ||
      !window.toolbar.visible ||
      lazy.PrivateBrowsingUtils.isWindowPrivate(window)
    ) {
      return;
    }

    let taskbarTabsButton = window.document.getElementById(kWidgetId);
    taskbarTabsButton.addEventListener("click", this, true);

    taskbarTabsButton.hidden = false;
  },

  async handleEvent(event) {
    let gBrowser = event.view.gBrowser;

    let win = Services.ww.openWindow(
      null,
      AppConstants.BROWSER_CHROME_URL,
      "_blank",
      kWebAppWindowFeatures,
      this._generateArgs(gBrowser.selectedTab)
    );

    await new Promise(resolve => {
      win.addEventListener("load", resolve, { once: true });
    });
    await win.delayedStartupPromise;
  },

  /**
   * Returns an array of args to pass into Services.ww.openWindow.
   * The first element will be the tab to be opened, the second element
   * will be a nsIWritablePropertyBag with a 'taskbartab' property
   *
   * The tab to be opened with the web app
   *
   * @param {MozTabbrowserTab} tab The tab we will open as a taskbar tab
   *
   * @returns {nsIMutableArray}
   */
  _generateArgs(tab) {
    let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    extraOptions.setPropertyAsBool("taskbartab", true);

    let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(tab);
    args.appendElement(extraOptions);
    args.appendElement(null);

    return args;
  },
};
