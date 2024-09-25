/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  UrlbarProviderOpenTabs: "resource:///modules/UrlbarProviderOpenTabs.sys.mjs",
});

export function getOpenTabs() {
  // We only want public tabs, so isInPrivateWindow = false
  let urls = lazy.UrlbarProviderOpenTabs.getOpenTabUrls(false);
  return Array.from(urls.keys());
}

export function switchToOpenTab(url) {
  // We only want public tabs, so skip private top windows
  let win = lazy.BrowserWindowTracker.getTopWindow({ private: false });
  win?.switchToTabHavingURI(url);
}
