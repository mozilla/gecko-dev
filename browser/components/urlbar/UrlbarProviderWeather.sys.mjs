/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderTopSites: "resource:///modules/UrlbarProviderTopSites.sys.mjs",
});

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 *
 * This provider is active only when either the Rust backend is disabled or
 * weather keywords are defined in Nimbus. When Rust is enabled and keywords are
 * not defined in Nimbus, the Rust component serves the initial weather
 * suggestion and UrlbarProviderQuickSuggest handles it along with other
 * suggestion types. Once the Rust backend is enabled by default and we no
 * longer want to experiment with weather keywords, this provider can be removed
 * along with the legacy telemetry it records.
 */
class ProviderWeather extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "Weather";
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  getPriority(context) {
    if (!context.searchString) {
      // Zero-prefix suggestions have the same priority as top sites.
      return lazy.UrlbarProviderTopSites.PRIORITY;
    }
    return super.getPriority(context);
  }

  /**
   * Whether this provider should be invoked for the given context.
   * If this method returns false, the providers manager won't start a query
   * with this provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @returns {boolean} Whether this provider should be invoked for the search.
   */
  isActive(queryContext) {
    // When Rust is enabled and keywords are not defined in Nimbus, weather
    // results are created by the quick suggest provider, not this one.
    if (
      lazy.UrlbarPrefs.get("quickSuggestRustEnabled") &&
      !lazy.QuickSuggest.weather?.keywords
    ) {
      return false;
    }

    // If the sources don't include search or the user used a restriction
    // character other than search, don't allow any suggestions.
    if (
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH) ||
      (queryContext.restrictSource &&
        queryContext.restrictSource != UrlbarUtils.RESULT_SOURCE.SEARCH)
    ) {
      return false;
    }

    if (queryContext.isPrivate || queryContext.searchMode) {
      return false;
    }

    let { keywords } = lazy.QuickSuggest.weather;
    if (!keywords) {
      return false;
    }

    return keywords.has(queryContext.trimmedLowerCaseSearchString);
  }

  /**
   * Starts querying. Extended classes should return a Promise resolved when the
   * provider is done searching AND returning results.
   *
   * @param {UrlbarQueryContext} queryContext The query context object
   * @param {Function} addCallback Callback invoked by the provider to add a new
   *        result. A UrlbarResult should be passed to it.
   * @returns {Promise}
   */
  async startQuery(queryContext, addCallback) {
    // As a reminder, this provider is not used and this method is not called
    // when Rust is enabled. UrlbarProviderQuickSuggest handles weather
    // suggestions when Rust is enabled.

    let result = await lazy.QuickSuggest.weather.makeResult(
      queryContext,
      null,
      queryContext.searchString
    );
    if (result) {
      result.payload.source = "merino";
      result.payload.provider = "accuweather";
      addCallback(this, result);
    }
  }

  getResultCommands(result) {
    return lazy.QuickSuggest.weather.getResultCommands(result);
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result
   *   The result whose view will be updated.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result) {
    return lazy.QuickSuggest.weather.getViewUpdate(result);
  }

  onEngagement(queryContext, controller, details) {
    this.#handlePossibleCommand(
      controller.view,
      details.result,
      details.selType
    );
  }

  #handlePossibleCommand(view, result, selType) {
    lazy.QuickSuggest.weather.handleCommand(view, result, selType);
  }
}

export var UrlbarProviderWeather = new ProviderWeather();
