/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  isProductURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  isSupportedSiteURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optedIn",
  "browser.shopping.experience2023.optedIn",
  null
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "autoClose",
  "browser.shopping.experience2023.autoClose.userEnabled",
  true
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasSeenNewPositionCard",
  "browser.shopping.experience2023.newPositionCard.hasSeen",
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
  static SIDEBAR_SETTINGS_ID = "viewCustomizeSidebar";
  static SHOW_NEW_POSITION_NOTIFICATION_CARD_EVENT_NAME =
    "ReviewCheckerManager:ShowNewPositionCard";

  #enabled = false;
  #hasListeners = null;
  #didAutoOpenForOptedInUser = false;

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
    this.#didAutoOpenForOptedInUser = null;
  }

  get optedIn() {
    return lazy.optedIn;
  }

  get hasSeenNewPositionCard() {
    return lazy.hasSeenNewPositionCard;
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
    this.window.addEventListener("OpenReviewCheckerSidebar", this);
    this.window.addEventListener("CloseReviewCheckerSidebar", this);
    this.window.addEventListener("DispatchNewPositionCardIfEligible", this);
    this.window.addEventListener(
      "ReverseSidebarPositionFromReviewChecker",
      this
    );
    this.window.addEventListener("ShowSidebarSettingsFromReviewChecker", this);
    this.#hasListeners = true;
  }

  #removeListeners() {
    if (!this.#hasListeners) {
      return;
    }
    this.window.gBrowser.removeProgressListener(this);
    this.window.removeEventListener("OpenReviewCheckerSidebar", this);
    this.window.removeEventListener("CloseReviewCheckerSidebar", this);
    this.window.removeEventListener("DispatchNewPositionCardIfEligible", this);
    this.window.removeEventListener(
      "ReverseSidebarPositionFromReviewChecker",
      this
    );
    this.window.removeEventListener(
      "ShowSidebarSettingsFromReviewChecker",
      this
    );
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

  /**
   * Reverses the position of the sidebar. If the sidebar is positioned on the left, it will
   * move to the right, and vice-versa.
   */
  reverseSidebarPositon() {
    if (
      !this.SidebarController.isOpen ||
      this.SidebarController.currentID !== ReviewCheckerManager.SIDEBAR_ID
    ) {
      return;
    }

    this.SidebarController.reversePosition();
  }

  /**
   * Displays the sidebar's settings panel.
   */
  showSidebarSettings() {
    if (
      !this.SidebarController.isOpen ||
      this.SidebarController.currentID !== ReviewCheckerManager.SIDEBAR_ID
    ) {
      return;
    }

    this.SidebarController.show(ReviewCheckerManager.SIDEBAR_SETTINGS_ID);
  }

  #canAutoOpen() {
    let isEligible = lazy.ShoppingUtils.isAutoOpenEligible();
    return isEligible && this.#enabled;
  }

  #canShowNewPositionCard() {
    return this.optedIn && !this.hasSeenNewPositionCard;
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

    this.#didAutoOpenForOptedInUser = false;

    let isSupportedSite = lazy.isSupportedSiteURL(aLocationURI);
    let isProductURL = lazy.isProductURL(aLocationURI);

    if (lazy.autoClose && !isSupportedSite && !isProductURL) {
      this.hideSidebar();
    }

    // If we have previously handled the location we should not do it again.
    if (selectedBrowser.isDistinctProductPageVisit) {
      return;
    }

    // Only auto-open the sidebar for a product page navigation.
    let isProductPageNavigation = lazy.ShoppingUtils.isProductPageNavigation(
      aLocationURI,
      aFlags
    );
    if (!isProductPageNavigation) {
      return;
    }

    // Record a product exposure for the location change.
    lazy.ShoppingUtils.recordExposure(aLocationURI, aFlags);

    let shouldAutoOpen;
    if (this.optedIn) {
      shouldAutoOpen = this.#canAutoOpen();
    } else {
      // Check if we should auto-open to allow opting in.
      shouldAutoOpen = lazy.ShoppingUtils.handleAutoActivateOnProduct();

      lazy.ShoppingUtils.sendTrigger({
        browser: selectedBrowser,
        id: "shoppingProductPageWithIntegratedRCSidebarClosed",
        context: {
          isReviewCheckerInSidebarClosed: !this.SidebarController?.isOpen,
        },
      });
    }

    // Only show sidebar if no sidebar panel is currently showing,
    // auto open is enabled and the RC sidebar is enabled.
    if (!this.SidebarController.isOpen && shouldAutoOpen) {
      this.showSidebar();

      if (this.optedIn) {
        this.#didAutoOpenForOptedInUser = true;
      }
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
        lazy.ShoppingUtils.sendTrigger({
          browser: this.window.gBrowser,
          id: "reviewCheckerSidebarClosedCallout",
          context: {
            isReviewCheckerInSidebarClosed: !this.SidebarController?.isOpen,
            isSidebarVisible: this.SidebarController._state?.launcherVisible,
          },
        });
        break;
      }
      case "ReverseSidebarPositionFromReviewChecker": {
        this.reverseSidebarPositon();
        break;
      }
      case "ShowSidebarSettingsFromReviewChecker": {
        this.showSidebarSettings();
        break;
      }
      case "DispatchNewPositionCardIfEligible": {
        /* Do not show the card if:
         * - the Review Checker panel was not opened via auto-open on a product page,
         * - if the user is not opted-in,
         * - or if the card was already displayed before. */
        if (
          !this.#didAutoOpenForOptedInUser ||
          !this.#canShowNewPositionCard()
        ) {
          return;
        }

        let showNewPositionCardEvent = new CustomEvent(
          ReviewCheckerManager.SHOW_NEW_POSITION_NOTIFICATION_CARD_EVENT_NAME,
          {
            bubbles: true,
            composed: true,
          }
        );
        this.window.dispatchEvent(showNewPositionCardEvent);
        break;
      }
    }
  }
}
