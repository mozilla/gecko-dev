/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  isSupportedSiteURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

/**
 * When a Review Checker sidebar panel is open ReviewCheckerParent
 * handles listening for location changes and determining if the
 * location or the current URI is a product or not.
 *
 * The ReviewCheckerChild will use that info to update the sidebar UI.
 *
 * This is a simplified version of the `ShoppingSidebarParent` for
 * using the shopping components in the main sidebar instead of the
 * custom shopping sidebar.
 */
export class ReviewCheckerParent extends JSWindowActorParent {
  static SHOPPING_OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";

  actorCreated() {
    this.topBrowserWindow = this.browsingContext.topChromeWindow;
    this.topBrowserWindow.gBrowser.addProgressListener(this);
  }

  didDestroy() {
    if (this.topBrowserWindow) {
      this.topBrowserWindow.gBrowser.removeProgressListener(this);
      this.topBrowserWindow = undefined;
    }
  }

  updateCurrentURL(uri, flags, isSupportedSite) {
    this.sendAsyncMessage("ShoppingSidebar:UpdateProductURL", {
      url: uri?.spec ?? null,
      isReload: !!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD),
      isSupportedSite,
    });
  }

  getCurrentURL() {
    let window = this.browsingContext.topChromeWindow;
    let { selectedBrowser } = window.gBrowser;
    let uri = selectedBrowser.currentURI;
    return uri.spec ?? null;
  }

  async receiveMessage(message) {
    if (this.browsingContext.usePrivateBrowsing) {
      throw new Error("We should never be invoked in PBM.");
    }
    switch (message.name) {
      case "GetProductURL":
        return this.getCurrentURL();
      case "DisableShopping":
        Services.prefs.setIntPref(
          ReviewCheckerParent.SHOPPING_OPTED_IN_PREF,
          2
        );
        break;
    }
    return null;
  }

  /**
   * Called by TabsProgressListener whenever any browser navigates from one
   * URL to another.
   * Note that this includes hash changes / pushState navigations, because
   * those can be significant for us.
   */
  onLocationChange(aWebProgress, _aRequest, aLocationURI, aFlags) {
    if (aWebProgress && !aWebProgress.isTopLevel) {
      return;
    }

    let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(
      this.topBrowserWindow
    );
    if (isPBM) {
      return;
    }

    lazy.ShoppingUtils.onLocationChange(aLocationURI, aFlags);

    this.updateCurrentURL(
      aLocationURI,
      aFlags,
      lazy.isSupportedSiteURL(aLocationURI)
    );
  }
}
