/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

const ABOUT_SHOPPING_SIDEBAR = "about:shoppingsidebar";
const ABOUT_BLANK = "about:blank";

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
  static CLOSE_SIDEBAR = "CloseReviewCheckerSidebar";
  static REVERSE_SIDEBAR = "ReverseSidebarPositionFromReviewChecker";
  static SHOW_SIDEBAR_SETTINGS = "ShowSidebarSettingsFromReviewChecker";
  static DISPATCH_NEW_POSITION_CARD_IF_ELIGIBLE =
    "DispatchNewPositionCardIfEligible";

  actorCreated() {
    this.topBrowserWindow = this.browsingContext.topChromeWindow;
    this.topBrowserWindow.gBrowser.addProgressListener(this);

    /* The manager class that tracks auto-open behaviour is likely to be instantiated before ReviewCheckerParent.
     * Once the parent actor is created, dispatch an event to the manager to see if we
     * can render the notification for the RC's new sidebar position. Also be sure to set up an
     * eventListener so that we can hear back from the manager. */
    this.showNewPositionCard = this.showNewPositionCard.bind(this);
    this.topBrowserWindow.addEventListener(
      "ReviewCheckerManager:ShowNewPositionCard",
      this.showNewPositionCard
    );
    this.dispatchNewPositionCardIfEligible();
  }

  didDestroy() {
    if (!this.topBrowserWindow) {
      return;
    }

    this.topBrowserWindow.removeEventListener(
      "ReviewCheckerManager:ShowNewPositionCard",
      this.showNewPositionCard
    );
    this.topBrowserWindow.gBrowser.removeProgressListener(this);
    this.topBrowserWindow = undefined;
  }

  /**
   * Returns true if a URL is ignored by ReviewChecker.
   *
   * @param {string} url the url that we want to validate.
   */
  static isIgnoredURL(url) {
    // about:shoppingsidebar is only used for testing with fake data.
    return url == ABOUT_SHOPPING_SIDEBAR || url == ABOUT_BLANK;
  }

  updateCurrentURL(uri, flags) {
    if (ReviewCheckerParent.isIgnoredURL(uri.spec)) {
      uri = null;
    }
    this.sendAsyncMessage("ReviewChecker:UpdateCurrentURL", {
      url: uri?.spec,
      isReload: !!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD),
    });
  }

  getCurrentURL() {
    let { selectedBrowser } = this.topBrowserWindow.gBrowser;
    let uri = selectedBrowser.currentURI;
    if (ReviewCheckerParent.isIgnoredURL(uri.spec)) {
      return null;
    }
    return uri?.spec;
  }

  showNewPositionCard() {
    this.sendAsyncMessage("ReviewChecker:ShowNewPositionCard");
  }

  async receiveMessage(message) {
    if (this.browsingContext.usePrivateBrowsing) {
      throw new Error("We should never be invoked in PBM.");
    }
    switch (message.name) {
      case "GetCurrentURL":
        return this.getCurrentURL();
      case "DisableShopping":
        Services.prefs.setIntPref(
          ReviewCheckerParent.SHOPPING_OPTED_IN_PREF,
          2
        );
        this.closeSidebarPanel();
        break;
      case "CloseShoppingSidebar":
        this.closeSidebarPanel();
        break;
      case "ReverseSidebarPosition":
        this.reverseSidebarPosition();
        break;
      case "ShowSidebarSettings":
        this.showSidebarSettings();
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

    this.updateCurrentURL(aLocationURI, aFlags);
  }

  dispatchEvent(eventName) {
    let event = new CustomEvent(eventName, {
      bubbles: true,
      composed: true,
    });
    this.topBrowserWindow.dispatchEvent(event);
  }

  closeSidebarPanel() {
    this.dispatchEvent(ReviewCheckerParent.CLOSE_SIDEBAR);
  }

  dispatchNewPositionCardIfEligible() {
    this.dispatchEvent(
      ReviewCheckerParent.DISPATCH_NEW_POSITION_CARD_IF_ELIGIBLE
    );
  }

  reverseSidebarPosition() {
    this.dispatchEvent(ReviewCheckerParent.REVERSE_SIDEBAR);
  }

  showSidebarSettings() {
    this.dispatchEvent(ReviewCheckerParent.SHOW_SIDEBAR_SETTINGS);
  }
}
