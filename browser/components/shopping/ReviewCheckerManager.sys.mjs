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
  #currentURI = null;

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
    this.#currentURI = null;
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
    this.window.gBrowser.addTabsProgressListener(this);
    this.window.gBrowser.tabContainer.addEventListener("TabSelect", this);
    this.window.addEventListener("OpenReviewCheckerSidebar", this);
    this.window.addEventListener("CloseReviewCheckerSidebar", this);
    this.window.addEventListener("DispatchNewPositionCardIfEligible", this);
    this.window.addEventListener(
      "ReverseSidebarPositionFromReviewChecker",
      this
    );
    this.window.addEventListener("ShowSidebarSettingsFromReviewChecker", this);
    this.window.addEventListener("SidebarShown", this);
    this.window.addEventListener("SidebarWillHide", this);
    this.#hasListeners = true;
  }

  #removeListeners() {
    if (!this.#hasListeners) {
      return;
    }
    this.window.gBrowser.removeTabsProgressListener(this);
    this.window.gBrowser.tabContainer.removeEventListener("TabSelect", this);
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
    let { selectedBrowser } = this.window.gBrowser;
    lazy.ShoppingUtils.clearWasClosedFlag(selectedBrowser);
    this.SidebarController.show(ReviewCheckerManager.SIDEBAR_ID);
  }

  /**
   * Hide the sidebar for the managed window if the Review Checker
   * is currently shown.
   *
   * @param {boolean} autoClosed
   *    true if the sidebar was auto-closed instead
   *    of by a user action.
   */
  hideSidebar(autoClosed) {
    if (
      !this.SidebarController.isOpen ||
      this.SidebarController.currentID !== ReviewCheckerManager.SIDEBAR_ID
    ) {
      return;
    }

    // Prevent auto-opening this product again on tab switch.
    if (!autoClosed && this.#canAutoOpen) {
      let { selectedBrowser } = this.window.gBrowser;
      selectedBrowser.reviewCheckerWasClosed = true;
    }

    this.SidebarController.hide();
  }

  /**
   * Checks if the sidebar is open to the Review Checker.
   */
  isSidebarOpen() {
    return (
      this.SidebarController.isOpen &&
      this.SidebarController.currentID == ReviewCheckerManager.SIDEBAR_ID
    );
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
   * Called by onLocationChange or onTabSelect to handle navigations
   * and tab switches.
   *
   * This is called on navigations in the current tab or as a
   * simulated change when switching tabs.
   *
   * Updates ShoppingUtils with the location changes.
   *
   * If a new location is a product url this will:
   *  - Check if the sidebar should be auto-opened.
   *  - Show the sidebar if needed.
   *
   * @param {nsIURI} aLocationURI
   *   The URI of the new location.
   * @param {integer} aFlags
   *   The reason for the location change.
   * @param {boolean} aIsSimulated
   *   True when change is from switching tabs
   *   and undefined otherwise.
   */
  locationChangeHandler(aLocationURI, aFlags, aIsSimulated) {
    this.#didAutoOpenForOptedInUser = false;

    let previousURI = this.#currentURI;
    let isSupportedSite = lazy.isSupportedSiteURL(aLocationURI);
    let isProductURL = lazy.isProductURL(aLocationURI);
    let { selectedBrowser } = this.window.gBrowser;

    // Only product URIs are needed for comparison.
    this.#currentURI = isProductURL ? aLocationURI : null;

    // Close the sidebar on tab switches if user has previously closed
    // the sidebar in this tab (for this location).
    let wasClosed = aIsSimulated && selectedBrowser.reviewCheckerWasClosed;

    let canAutoClose = this.#canAutoOpen() && !isSupportedSite && !isProductURL;
    if (canAutoClose || wasClosed) {
      this.hideSidebar(true);
      return;
    }

    // Check if this is a new location.
    let hasLocationChanged = lazy.ShoppingUtils.hasLocationChanged(
      aLocationURI,
      aFlags,
      previousURI
    );

    // Only auto-open the sidebar for a product page.
    if (!isProductURL || !hasLocationChanged) {
      return;
    }

    // Allow this tab to auto-open again if this was a navigation
    // to a new product.
    if (!aIsSimulated) {
      lazy.ShoppingUtils.clearWasClosedFlag(selectedBrowser);
    }

    // If the sidebar is currently open there is no need to auto-open,
    // and the ReviewCheckerParent will handle recording the exposure.
    let isSidebarOpen = this.isSidebarOpen();
    if (isSidebarOpen) {
      return;
    }

    let shouldAutoOpen;
    if (this.optedIn) {
      /**
       * We can auto-open the sidebar if:
       * - Auto-open is enabled.
       * - This sidebar has not been closed for this product in this tab.
       * - This is a navigation to a product page, or the location change
       *   is from switching to this tab.
       * - The user has not had a chance to opt-in or out yet.
       */
      shouldAutoOpen = this.#canAutoOpen() && !wasClosed;
    } else {
      // Check if we should auto-open to allow opting in.
      shouldAutoOpen = lazy.ShoppingUtils.handleAutoActivateOnProduct();
      // Only trigger the callout if the panel is not auto-opening
      if (!shouldAutoOpen) {
        lazy.ShoppingUtils.sendTrigger({
          browser: selectedBrowser,
          id: "shoppingProductPageWithIntegratedRCSidebarClosed",
          context: {
            isReviewCheckerInSidebarClosed: !this.SidebarController?.isOpen,
          },
        });
      }
    }

    // Only show sidebar if no sidebar panel is currently showing,
    // auto open is enabled and the RC sidebar is enabled.
    if (!this.SidebarController.isOpen && shouldAutoOpen) {
      this.showSidebar();

      if (this.optedIn) {
        this.#didAutoOpenForOptedInUser = true;
      }

      return;
    }

    // Record a product exposure for the location change,
    // if the sidebar is not going to open and record it.
    if (!aIsSimulated) {
      lazy.ShoppingUtils.recordExposure(aLocationURI, aFlags);
      lazy.ShoppingUtils.clearIsDistinctProductPageVisitFlag(selectedBrowser);
    }
  }

  /**
   * Listens to TabsProgressListener and handles new top level
   * location changes in any browser in the window.
   *
   * Calls locationChangeHandler with the location changes.
   *
   * If a background tab navigated to a location that is a product url,
   * this will mark the browser with `isDistinctProductPageVisit=true`
   * so that navigation can be handled when the background tab is selected.
   *
   *  @param {Browser} aBrowser
   *   The browser the location change took place in.
   * @param {nsIWebProgress} aWebProgress
   *   The nsIWebProgress instance that fired the notification.
   * @param {nsIRequest} _aRequest
   *  Unused and is null when change is simulated.
   * @param {nsIURI} aLocationURI
   *   The URI of the new location.
   * @param {integer} aFlags
   *   The reason for the location change.
   */
  onLocationChange(aBrowser, aWebProgress, _aRequest, aLocationURI, aFlags) {
    if (!aWebProgress.isTopLevel) {
      return;
    }

    let { selectedBrowser } = this.window.gBrowser;
    // No need to handle background navigations until they
    // are foregrounded.
    if (aBrowser != selectedBrowser) {
      let isProductURL = lazy.isProductURL(aLocationURI);
      if (isProductURL) {
        // Mark this product page to be handled in the future.
        aBrowser.isDistinctProductPageVisit = true;
      } else {
        // Otherwise this is no longer a product page that
        // needs to be handled.
        lazy.ShoppingUtils.clearIsDistinctProductPageVisitFlag(selectedBrowser);
      }
      return;
    }

    this.locationChangeHandler(aLocationURI, aFlags, false);
  }

  /**
   * Listens for changes to the selected tab.
   *
   * Calls locationChangeHandler with URI of the current tab.
   *
   * If this tabs location changed when not selected, the location will be
   * handled like a navigation and `isDistinctProductPageVisit` will be removed
   * by the ReviewCheckerParent after the sidebar has been updated.
   *
   * Otherwise the change will be sent as a simulated change to represent that it
   * is just a tab switch as `gBrowser.onProgressListener` does.
   */
  onTabSelect() {
    let { selectedBrowser, currentURI } = this.window.gBrowser;

    let isSimulated = !selectedBrowser.isDistinctProductPageVisit;
    this.#currentURI = null;

    this.locationChangeHandler(currentURI, null, isSimulated);
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
      case "TabSelect": {
        this.onTabSelect();
        break;
      }
      case "SidebarShown": {
        // Clear that the sidebar was closed as it has opened
        // to the ReviewChecker again.
        if (
          this.SidebarController.isOpen ||
          this.SidebarController.currentID == ReviewCheckerManager.SIDEBAR_ID
        ) {
          let { selectedBrowser } = this.window.gBrowser;
          lazy.ShoppingUtils.clearWasClosedFlag(selectedBrowser);
        }
        break;
      }
      case "SidebarWillHide": {
        lazy.ShoppingUtils.sendTrigger({
          browser: this.browser,
          id: "reviewCheckerSidebarClosedCallout",
          context: {
            isReviewCheckerInSidebarClosed: !this.SidebarController?.isOpen,
            isSidebarVisible: this.SidebarController.launcherVisible,
          },
        });
      }
    }
  }
}
