/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { RemotePageChild } from "resource://gre/actors/RemotePageChild.sys.mjs";

import {
  ShoppingProduct,
  isProductURL,
  isSupportedSiteURL,
} from "chrome://global/content/shopping/ShoppingProduct.mjs";

let lazy = {};

let gAllActors = new Set();

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "optedIn",
  "browser.shopping.experience2023.optedIn",
  null,
  function optedInStateChanged() {
    for (let actor of gAllActors) {
      actor.optedInStateChanged();
    }
  }
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "adsEnabled",
  "browser.shopping.experience2023.ads.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "adsEnabledByUser",
  "browser.shopping.experience2023.ads.userEnabled",
  true,
  function adsEnabledByUserChanged() {
    for (let actor of gAllActors) {
      actor.adsEnabledByUserChanged();
    }
  }
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "autoOpenEnabled",
  "browser.shopping.experience2023.autoOpen.enabled",
  true
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "autoOpenEnabledByUser",
  "browser.shopping.experience2023.autoOpen.userEnabled",
  true,
  function autoOpenEnabledByUserChanged() {
    for (let actor of gAllActors) {
      actor.autoOpenEnabledByUserChanged();
    }
  }
);

/**
 * The ReviewCheckerChild will get the current URL from the parent
 * and will request data to update the sidebar UI if that URL is a
 * product or display the current opt-in or empty state.
 */
export class ReviewCheckerChild extends RemotePageChild {
  constructor() {
    super();
  }

  actorCreated() {
    super.actorCreated();
    gAllActors.add(this);
  }

  didDestroy() {
    this._destroyed = true;
    super.didDestroy?.();
    gAllActors.delete(this);
    this.#product?.uninit();
  }

  #currentURI = null;
  #product = null;

  get currentURL() {
    return this.#currentURI?.spec;
  }

  get canFetchAndShowData() {
    return lazy.optedIn === 1;
  }

  get adsEnabled() {
    return lazy.adsEnabled;
  }

  get adsEnabledByUser() {
    return lazy.adsEnabledByUser;
  }

  get canFetchAndShowAd() {
    return this.adsEnabled && this.adsEnabledByUser;
  }

  get autoOpenEnabled() {
    return lazy.autoOpenEnabled;
  }

  get autoOpenEnabledByUser() {
    return lazy.autoOpenEnabledByUser;
  }

  receiveMessage(message) {
    if (this.browsingContext.usePrivateBrowsing) {
      throw new Error("We should never be invoked in PBM.");
    }
    switch (message.name) {
      case "ReviewChecker:UpdateCurrentURL":
        this.locationChanged(message.data);
        break;
    }
    return null;
  }

  handleEvent(event) {
    let aid, sponsored, product;
    switch (event.type) {
      case "ContentReady":
        this.resetContent();
        this.setLocation();
        break;
      case "PolledRequestMade":
        product = this.getProductForURI(this.#currentURI);
        this.updateProductData(product, { isPolledRequest: true });
        break;
      case "ReportProductAvailable":
        product = this.getProductForURI(this.#currentURI);
        this.reportProductAvailable(product);
        break;
      case "AdClicked":
        aid = event.detail.aid;
        sponsored = event.detail.sponsored;
        ShoppingProduct.sendAttributionEvent("click", aid);
        Glean.shopping.surfaceAdsClicked.record({ sponsored });
        break;
      case "AdImpression":
        aid = event.detail.aid;
        sponsored = event.detail.sponsored;
        ShoppingProduct.sendAttributionEvent("impression", aid);
        Glean.shopping.surfaceAdsImpression.record({ sponsored });
        break;
      case "DisableShopping":
        this.sendAsyncMessage("DisableShopping");
        break;
      case "CloseShoppingSidebar":
        this.sendAsyncMessage("CloseShoppingSidebar");
        break;
    }
  }

  /**
   * Exposed for testing to set the private currentURI.
   *
   * @param {nsIURI} uri
   */
  set currentURI(uri) {
    if (!(uri instanceof Ci.nsIURI)) {
      throw new Error("currentURI setter expects an nsIURI");
    }
    this.#currentURI = uri;
  }

  /**
   * Get the URI the content has been set to.
   *
   * @returns {nsIURI} currentURI
   */
  get currentURI() {
    return this.#currentURI;
  }

  /**
   * Exposed for testing to set the private product.
   *
   * TODO: Bug 1927956 - This will need to get the URI for the product once we are
   * caching them.
   *
   * @param {ShoppingProduct} product
   */
  set product(product) {
    if (!(product instanceof ShoppingProduct)) {
      throw new Error("product setter expects an instance of ShoppingProduct");
    }
    this.#product = product;
  }

  /**
   * Get or create a product for the given URI.
   *
   * @param {nsIURI} productURI
   * @returns {ShoppingProduct}
   */
  getProductForURI(productURI) {
    // TODO: Bug 1927956 - Check a product is cached for this URI.
    if (this.#product) {
      return this.#product;
    }
    let product = new ShoppingProduct(productURI);
    // TODO: Bug 1927956 - Add to product cache for this URI.
    this.#product = product;
    return product;
  }

  /**
   * Check check if URIs represent the same product by
   * comparing URLs and then parsed product ID.
   *
   * @param {nsIURI} newURI
   * @param {nsIURI} currentURI
   * @returns {boolean}
   */
  isSameProduct(newURI, currentURI) {
    if (!newURI || !currentURI) {
      return false;
    }

    // Check if the URIs are equal:
    if (currentURI.equalsExceptRef(newURI)) {
      return true;
    }

    let product = this.getProductForURI(currentURI);
    if (!product) {
      return false;
    }

    // If the current ShoppingProduct has product info set,
    // check if the product ids are the same:
    let currentProduct = product.product;
    if (currentProduct) {
      let newProduct = ShoppingProduct.fromURL(URL.fromURI(newURI));
      if (newProduct.id === currentProduct.id) {
        return true;
      }
    }

    return false;
  }

  /**
   * Reset the content and update for the current URI.
   *
   * @returns {Promise<undefined>}
   */
  async optedInStateChanged() {
    // Clear the current content
    this.resetContent({ focusCloseButton: true });

    // Get a URI if we don't have one yet.
    if (!this.#currentURI) {
      await this.setLocation();
    }

    await this.updateContent(this.#currentURI);
  }

  /**
   * Get recommendations for the current ShoppingProduct
   * if enabled or remove current recommendations.
   *
   * @returns {Promise<undefined>}
   */
  async adsEnabledByUserChanged() {
    this.updateAdsEnabledByUser(this.adsEnabledByUser);

    if (!this.canFetchAndShowAd) {
      return;
    }

    let product = this.getProductForURI(this.#currentURI);
    await this.updateRecommendations(product);
  }

  /**
   * Update auto-open to user's pref value.
   *
   */
  autoOpenEnabledByUserChanged() {
    this.updateAutoOpenEnabledByUser(this.autoOpenEnabledByUser);
  }

  /**
   * Get current URL for the parent and update the location to it.
   *
   * @returns {Promise<undefined>}
   */
  async setLocation() {
    // check if can fetch and show data
    let url = await this.sendQuery("GetCurrentURL");

    // Bail out if we opted out in the meantime, or don't have a URI.
    if (!this.canContinue(null, false)) {
      return;
    }

    await this.locationChanged({ url });
  }

  /**
   * Update the currentURI to a new location.
   *
   * For a new location this will:
   * - Reset remove the ShoppingProduct for the previous URI.
   * - Update the content for the new URI.
   *
   * @param {object?} options
   * @param {bool} options.url
   * @param {bool} [options.isReload]
   *
   * @returns {Promise<undefined>}
   */
  async locationChanged({ url, isReload } = {}) {
    let uri = url ? Services.io.newURI(url) : null;

    // If we're going from null to null, bail out:
    if (!this.#currentURI && !uri) {
      return;
    }

    // If we haven't reloaded, check if the URIs represent the same product
    // as sites might change the URI after they have loaded (Bug 1852099).
    if (!isReload && this.isSameProduct(uri, this.#currentURI)) {
      return;
    }

    this.#product?.uninit();
    this.#product = null;

    this.#currentURI = uri;

    await this.updateContent(uri);
  }

  /**
   * Re-renders the content whenever whenever the location changes.
   *
   * The expected cases for this are:
   * - page navigations (both to new products and away from a product once
   *   the sidebar has been created)
   * - opt in state changes.
   *
   * For a new location this will:
   * - Check if the location is a product page or supported site.
   * - Update the content location and empty states.
   * - Get or create a new ShoppingProduct for the URI if needed.
   * - Update state and data for that product.
   * - Update recommendation for that product if enabled.
   *
   * @param {nsIURI} uri
   *
   * @returns {Promise<undefined>}
   */
  async updateContent(uri) {
    if (this._destroyed || !uri) {
      return;
    }

    let isProductPage = isProductURL(uri);

    if (!this.canFetchAndShowData) {
      this.showOnboarding({ productUrl: uri.spec });
      return;
    }

    if (isProductPage) {
      let product = this.getProductForURI(uri);

      // We want to update the location to clear out the content from
      // the previous URL immediately, without waiting for potentially
      // async operations like obtaining product information.
      this.updateLocation({ isProductPage });

      await this.updateProductData(product);

      if (this.canShowAds(uri)) {
        await this.updateRecommendations(product);
      }
    } else {
      let isSupportedSite = isSupportedSiteURL(uri);
      let supportedDomains = ShoppingProduct.getSupportedDomains();
      // If the URI is not a product page, we should display an empty state.
      // That empty state could be for either a support or unsupported site.
      this.updateLocation({ isProductPage, isSupportedSite, supportedDomains });
    }
  }

  /**
   * updateContent is an async function, and when we're off making requests or doing
   * other things asynchronously, the actor can be destroyed, the user
   * might navigate to a new page, the user might disable the feature ... -
   * all kinds of things can change. So we need to repeatedly check
   * whether we can keep going with our async processes. This helper takes
   * care of these checks.
   *
   * @param {nsIURI} currentURI
   * @param {boolean} [checkURI] = true
   *
   * @returns {boolean}
   */
  canContinue(currentURI, checkURI = true) {
    if (this._destroyed || !this.canFetchAndShowData) {
      return false;
    }
    if (!checkURI) {
      return true;
    }
    return currentURI && currentURI == this.#currentURI;
  }

  /**
   * Utility function to determine if we should display ads. This is different
   * from fetching ads, because of ads exposure telemetry (bug 1858470).
   *
   * @param {nsIURI} uri
   *
   * @returns {boolean}
   */
  canShowAds(uri) {
    return (
      uri.equalsExceptRef(this.#currentURI) &&
      this.canFetchAndShowData &&
      this.canFetchAndShowAd
    );
  }

  /**
   * Async helper that will request data from the Fakespot API for a passed product
   * and update the content with the new state.
   *
   * Records telemetry if reviews are not available for the product.
   *
   * @param {ShoppingProduct} product
   * @param {object} options
   * @param {boolean} [options.isPolledRequest=false]
   */
  async updateProductData(product, { isPolledRequest = false } = {}) {
    let uri = this.#currentURI;
    let productUrl = uri?.spec;
    let analysisStatusResponse;
    let data;
    try {
      if (isPolledRequest) {
        // Request a new analysis.
        analysisStatusResponse = await product.requestCreateAnalysis();
      } else {
        // Request the current analysis status.
        analysisStatusResponse = await product.requestAnalysisCreationStatus();
      }
      // Status will be "not_found" if the current analysis is up-to-date.
      let analysisStatus = analysisStatusResponse?.status ?? "not_found";

      let isAnalysisInProgress =
        ShoppingProduct.isAnalysisInProgress(analysisStatus);
      if (isAnalysisInProgress) {
        // Do not clear data however if an analysis was requested via a call-to-action.
        if (!isPolledRequest) {
          this.updateAnalysisStatus({ analysisStatus });
        }

        analysisStatus = await this.waitForAnalysisCompleted(
          product,
          analysisStatus
        );
      }

      this.updateAnalysisStatus({ analysisStatus });

      let hasAnalysisCompleted =
        ShoppingProduct.hasAnalysisCompleted(analysisStatus);
      if (!hasAnalysisCompleted) {
        return;
      }

      data = await product.requestAnalysis();
    } catch (err) {
      console.warn("Failed to fetch product analysis data", err);
      data = { error: err };
    }

    // Check if we got nuked from orbit, or the product URI or opt in changed while we waited.
    if (!data || !this.canContinue(uri)) {
      return;
    }

    this.sendToContent("Update", {
      productUrl,
      data,
      showOnboarding: false,
    });

    if (!isPolledRequest && !data.error && !data.grade) {
      Glean.shopping.surfaceNoReviewReliabilityAvailable.record();
    }
  }

  /**
   * Async helper that will request recommendation data from the Fakespot API for a passed product
   * and update the content with the new state.
   *
   * Records telemetry if recommendation are placed or not available for the product.
   *
   * @param {ShoppingProduct} product
   */
  async updateRecommendations(product) {
    let uri = this.#currentURI;
    let recommendationData;
    try {
      recommendationData = await product.requestRecommendations();
    } catch (err) {
      console.warn("Failed to fetch product recommendations data", err);
      recommendationData = [];
    }
    // Check if the product URI or opt in changed while we waited.
    if (!this.canShowAds(uri)) {
      return;
    }

    if (!recommendationData.length) {
      // We tried to fetch an ad, but didn't get one.
      Glean.shopping.surfaceNoAdsAvailable.record();
    } else {
      let sponsored = recommendationData[0].sponsored;

      ShoppingProduct.sendAttributionEvent(
        "placement",
        recommendationData[0].aid
      );

      Glean.shopping.surfaceAdsPlacement.record({
        sponsored,
      });
    }

    this.sendToContent("UpdateRecommendations", {
      recommendationData,
    });
  }

  /**
   * Async helper that will report a product is now available to the Fakespot API.
   *
   * @param {ShoppingProduct} product
   */
  async reportProductAvailable(product) {
    await product.sendReport();
  }

  /**
   * Async helper that will poll the Fakespot API until a product's analysis is no longer
   * in progress, this could complete or fail as with a status.
   *
   * Callback updates the analysis progress as reported by the Fakespot API.
   *
   * @param {ShoppingProduct} product
   */
  async waitForAnalysisCompleted(product, analysisStatus) {
    try {
      let analysisStatusResponse = await product.pollForAnalysisCompleted(
        {
          pollInitialWait: analysisStatus == "in_progress" ? 0 : undefined,
        },
        progress => {
          this.sendToContent("UpdateAnalysisProgress", {
            progress,
          });
        }
      );
      return analysisStatusResponse.status;
    } catch (err) {
      console.warn("Failed to get product status", err);
      return analysisStatus;
    }
  }

  /**
   * Content state helper to send preference changes to the shopping-container
   * and clear the current state.
   *
   * @param {object?} options
   * @param {bool} [options.focusCloseButton=false]
   */
  resetContent({ focusCloseButton = false } = {}) {
    this.sendToContent("Update", {
      adsEnabled: lazy.adsEnabled,
      adsEnabledByUser: lazy.adsEnabledByUser,
      autoOpenEnabled: lazy.autoOpenEnabled,
      autoOpenEnabledByUser: lazy.autoOpenEnabledByUser,
      showOnboarding: !this.canFetchAndShowData,
      data: null,
      recommendationData: null,
      focusCloseButton,
    });
  }

  /**
   * Content state helper to send analysis status changes to the shopping-container.
   *
   * @param {object?} options
   * @param {string} options.analysisStatus
   */
  updateAnalysisStatus({ analysisStatus } = {}) {
    let data;
    // Use the analysis status instead of re-requesting unnecessarily,
    // or throw if the status from the last analysis was an error.
    switch (analysisStatus) {
      case "not_analyzable":
      case "page_not_supported":
        data = { page_not_supported: true };
        break;
      case "not_enough_reviews":
        data = { not_enough_reviews: true };
        break;
      case "unprocessable":
      case "stale":
        data = { error: new Error(analysisStatus, { cause: analysisStatus }) };
        break;
      default:
      // Status is "completed" or "not_found" (no analysis status),
      // so we should request the analysis data.
    }

    let isAnalysisInProgress =
      ShoppingProduct.isAnalysisInProgress(analysisStatus);
    if (!data && !isAnalysisInProgress) {
      return;
    }

    this.sendToContent("Update", {
      data,
      isAnalysisInProgress,
    });
  }

  /**
   * Content state helper to update the current location in the content.
   *
   * @param {object?} options
   * @param {bool} [options.isProductPage=false] If the location has a product or not.
   * @param {bool} [options.isSupportedSite=false] If the location is on a supported site or not.
   */
  updateLocation({ isProductPage = true, isSupportedSite = false } = {}) {
    this.sendToContent("Update", {
      isProductPage,
      isSupportedSite,
    });
  }

  /**
   * Shows the onboarding flow in the content.
   *
   * @param {object?} options
   * @param {string} [options.productUrl] URL of the current product if this is a product page.
   */
  showOnboarding({ productUrl } = {}) {
    // Don't bother continuing if the user has opted out.
    if (lazy.optedIn == 2) {
      return;
    }

    // Similar to canContinue() above, check to see if things
    // have changed while we were waiting. Bail out if the user
    // opted in, or if the actor doesn't exist.
    if (this._destroyed || this.canFetchAndShowData) {
      return;
    }

    // Send the productUrl to content for Onboarding's dynamic text
    this.sendToContent("Update", {
      showOnboarding: true,
      data: null,
      productUrl,
    });
  }

  /**
   * Updates if recommendation have been enabled or disable in the content settings.
   *
   * @param {bool} adsEnabledByUser
   */
  updateAdsEnabledByUser(adsEnabledByUser) {
    this.sendToContent("adsEnabledByUserChanged", {
      adsEnabledByUser,
    });
  }

  /**
   * Updates if auto open has been enabled or disable in the content settings.
   *
   * @param {bool} autoOpenEnabledByUser
   */
  updateAutoOpenEnabledByUser(autoOpenEnabledByUser) {
    this.sendToContent("autoOpenEnabledByUserChanged", {
      autoOpenEnabledByUser,
    });
  }

  /**
   * Updates percentage complete for a product analysis.
   *
   * @param {number} progress
   */
  updateAnalysisProgress(progress) {
    this.sendToContent("UpdateAnalysisProgress", {
      progress,
    });
  }

  /**
   * Send messages and cloned objects to the content.
   *
   * @param {string} eventName event string to pass.
   * @param {object} detail object to clone.
   */
  sendToContent(eventName, detail) {
    if (this._destroyed) {
      return;
    }
    let win = this.contentWindow;
    let evt = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(evt);
  }
}
