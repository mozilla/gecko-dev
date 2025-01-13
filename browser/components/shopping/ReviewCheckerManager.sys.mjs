/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  isProductURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optedIn",
  "browser.shopping.experience2023.optedIn",
  null
);

/**
 * Manages opening and closing the Review Checker in the sidebar.
 * Listens for location changes and will auto-open if enabled.
 */
export class ReviewCheckerManager {
  static SIDEBAR_ID = "viewReviewCheckerSidebar";
  static SIDEBAR_TOOL = "reviewchecker";
  static SIDEBAR_ENABLED_PREF = "reviewchecker";

  #enabled = false;
  #hasListeners = null;

  /**
   * Creates manager to open and close the review checker sidebar
   * in a passed window.
   *
   * @param {Window} win
   */
  constructor(win) {
    let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(win);
    if (isPBM) {
      return;
    }
    this.window = win;
    this.SidebarController = win.SidebarController;
    this.init();
  }

  /**
   * Start listening for changes to the
   * enabled sidebar tools.
   */
  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "sidebarTools",
      "sidebar.main.tools",
      null,
      () => {
        this.#updateSidebarEnabled();
      }
    );

    this.#updateSidebarEnabled();
  }

  /**
   * Remove any listeners.
   */
  uninit() {
    this.#removeListeners();
  }

  #updateSidebarEnabled() {
    // Add listeners if "reviewchecker" is enabled in the sidebar tools.
    this.#enabled = this.sidebarTools.includes(
      ReviewCheckerManager.SIDEBAR_TOOL
    );
    if (this.#enabled) {
      this.#addListeners();
    } else {
      this.#removeListeners();
    }
  }

  #addListeners() {
    if (this.#hasListeners) {
      return;
    }
    this.window.gBrowser.addProgressListener(this);
    this.window.addEventListener("ShowReviewCheckerSidebar", this);
    this.window.addEventListener("CloseReviewCheckerSidebar", this);
    this.#hasListeners = true;
  }

  #removeListeners() {
    if (!this.#hasListeners) {
      return;
    }
    this.window.gBrowser.removeProgressListener(this);
    this.window.removeEventListener("ShowReviewCheckerSidebar", this);
    this.window.removeEventListener("CloseReviewCheckerSidebar", this);
    this.#hasListeners = null;
  }

  /**
   * Show the Review Checker in the sidebar for the managed window.
   */
  showSidebar() {
    this.SidebarController.show(ReviewCheckerManager.SIDEBAR_ID);
  }

  /**
   * Hide the sidebar for the managed window if the Review Checker
   * is currently shown.
   */
  hideSidebar() {
    if (
      !this.SidebarController.isOpen ||
      this.SidebarController.currentID !== ReviewCheckerManager.SIDEBAR_ID
    ) {
      return;
    }

    this.SidebarController.hide();
  }

  #canAutoOpen() {
    let isEligible = lazy.ShoppingUtils.isAutoOpenEligible();
    return isEligible && this.#enabled;
  }

  /**
   * Listens to location changes from tabbrowser and handles new top level
   * location changes and tab switches.
   *
   * This is called on navigation in the current tab and fires a simulated
   * change when switching to a different tab.
   *
   * Updates ShoppingUtils with the location changes.
   *
   * If a new location is a product url this will:
   *  - Check if the sidebar should be auto-opened.
   *  - Show the sidebar if needed.
   *  - Mark the browser with `isDistinctProductPageVisit=true` so that
   *    each navigation is only handled once.
   *
   * @param {nsIWebProgress} aWebProgress
   *   The nsIWebProgress instance that fired the notification.
   * @param {nsIRequest} _aRequest
   *  Unused and is null when change is simulated.
   * @param {nsIURI} aLocationURI
   *   The URI of the new location.
   * @param {integer} aFlags
   *   The reason for the location change.
   * @param {boolean} aIsSimulated
   *   True when change is from switching tabs
   *   and undefined otherwise.
   */
  onLocationChange(
    aWebProgress,
    _aRequest,
    aLocationURI,
    aFlags,
    aIsSimulated
  ) {
    if (aWebProgress && !aWebProgress.isTopLevel) {
      return;
    }

    let { selectedBrowser } = this.window.gBrowser;
    // If this was not a tab change, clear any previously set product visit
    // as we have navigated to a new location.
    if (!aIsSimulated && selectedBrowser.isDistinctProductPageVisit) {
      delete selectedBrowser.isDistinctProductPageVisit;
    }

    // If we have previously handled the location we should not do it again.
    if (selectedBrowser.isDistinctProductPageVisit) {
      return;
    }

    // Record the location change.
    lazy.ShoppingUtils.onLocationChange(aLocationURI, aFlags);

    // Only auto-open the sidebar for locations that are products.
    if (!lazy.isProductURL(aLocationURI)) {
      return;
    }

    let shouldAutoOpen;
    if (lazy.optedIn) {
      shouldAutoOpen = this.#canAutoOpen();
    } else {
      // Check if we should auto-open to allow opting in.
      shouldAutoOpen = lazy.ShoppingUtils.handleAutoActivateOnProduct();
    }

    // Only show sidebar if no sidebar panel is currently showing,
    // auto open is enabled and the RC sidebar is enabled.
    if (!this.SidebarController.isOpen && shouldAutoOpen) {
      this.showSidebar();
    }

    // Mark product location as distinct the first time we see it.
    selectedBrowser.isDistinctProductPageVisit = true;
  }

  handleEvent(event) {
    switch (event.type) {
      case "OpenReviewCheckerSidebar": {
        this.showSidebar();
        break;
      }
      case "CloseReviewCheckerSidebar": {
        this.hideSidebar();
        break;
      }
    }
  }
}
