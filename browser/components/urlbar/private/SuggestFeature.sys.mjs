/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-unused-vars */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * Base class for Suggest features. It can be extended to implement a feature
 * that should be enabled only when Suggest is enabled. Most features should
 * extend one of the `SuggestFeature` subclasses, however.
 *
 * Subclasses should be registered with `QuickSuggest` by adding them to the
 * `FEATURES` const in `QuickSuggest.sys.mjs`.
 */
export class SuggestFeature {
  // Methods designed for overriding below

  /**
   * @returns {Array}
   *   If the feature is conditioned on any prefs or Nimbus variables, the
   *   subclass should override this getter and return their names in this array
   *   so that `update()` and `enable()` can be called when they change. Names
   *   should be recognized by `UrlbarPrefs`, i.e., pref names should be
   *   relative to the `browser.urlbar.` branch. For Nimbus variables with
   *   fallback prefs, include only the variable name.
   *
   *   When Suggest determines whether the feature should be enabled, it will
   *   call `UrlbarPrefs.get()` on each name in this array and disable the
   *   feature if any are falsey. If any of the prefs or variables are not
   *   booleans, the subclass may also need to override
   *   `additionalEnablingPredicate` to perform additional checks on them.
   */
  get enablingPreferences() {
    return [];
  }

  /**
   * @returns {string | null}
   *   If there is a feature-specific pref that is controlled by the user and
   *   toggles the feature on and off, the subclass should override this getter
   *   and return its name. It should also be included in `enablingPreferences`.
   *   The name should be recognized by `UrlbarPrefs`, i.e., it should be
   *   relative to the `browser.urlbar.` branch.
   *
   *   If the feature is a `SuggestProvider`, typically this should be the pref
   *   that's named `suggest.mySuggestionType` and set to `false` when the user
   *   dismisses the entire suggestion type, i.e., the relevant
   *   `browser.urlbar.suggest.` pref.
   *
   *   The pref should be controlled by the user, so it should never be the
   *   feature's feature-gate pref.
   *
   *   The pref should control this feature specifically, so it should never be
   *   `suggest.quicksuggest.sponsored` or `suggest.quicksuggest.nonsponsored`.
   *   If the feature has no such pref, this getter should return null.
   */
  get primaryUserControlledPreference() {
    return null;
  }

  /**
   * @returns {boolean}
   *   If the feature is conditioned on any predicate other than the prefs and
   *   Nimbus variables in `enablingPreferences`, the subclass should override
   *   this getter and return whether the feature should be enabled. It may also
   *   need to override this getter if any of the prefs or variables in
   *   `enablingPreferences` are not booleans so that it can perform additional
   *   checks on them. (The predicate does not need to check prefs and variables
   *   in `enablingPreferences` that are booleans.)
   *
   *   This getter will be called only when Suggest is enabled and all prefs and
   *   variables in `enablingPreferences` are truthy.
   */
  get additionalEnablingPredicate() {
    return true;
  }

  /**
   * This method should initialize or uninitialize any state related to the
   * feature. It will only be called when the enabled status changes, i.e., when
   * it goes from false to true or true to false.
   *
   * @param {boolean} enabled
   *   Whether the feature should be enabled or not.
   */
  enable(enabled) {}

  // Methods not designed for overriding below

  /**
   * @returns {boolean}
   *   Whether the feature should be enabled, assuming Suggest is enabled.
   */
  get shouldEnable() {
    return (
      this.enablingPreferences.every(p => lazy.UrlbarPrefs.get(p)) &&
      this.additionalEnablingPredicate
    );
  }

  /**
   * @returns {ConsoleInstance}
   *   The feature's logger.
   */
  get logger() {
    if (!this._logger) {
      this._logger = lazy.UrlbarUtils.getLogger({
        prefix: `QuickSuggest.${this.name}`,
      });
    }
    return this._logger;
  }

  /**
   * @returns {boolean}
   *   Whether the feature is enabled. The enabled status is automatically
   *   managed by `QuickSuggest` and subclasses should not override this.
   */
  get isEnabled() {
    return this.#isEnabled;
  }

  /**
   * @returns {string}
   *   The feature's name.
   */
  get name() {
    return this.constructor.name;
  }

  /**
   * Enables or disables the feature according to `shouldEnable` and whether
   * Suggest is enabled. If the feature's enabled status changes, `enable()` is
   * called with the new status; otherwise `enable()` is not called.
   */
  update() {
    let enable =
      lazy.UrlbarPrefs.get("quickSuggestEnabled") && this.shouldEnable;
    if (enable != this.isEnabled) {
      this.logger.info("Feature enabled status changed", {
        nowEnabled: enable,
      });
      this.#isEnabled = enable;
      this.enable(enable);
    }
  }

  #isEnabled = false;
}

/**
 * Base class for Suggest features that manage a suggestion type [1].
 *
 * The same suggestion type can be served by multiple backends, and a single
 * `SuggestProvider` subclass can manage the type regardless of backend by
 * overriding the appropriate methods and getters.
 *
 * Subclasses should be registered with `QuickSuggest` by adding them to the
 * `FEATURES` const in `QuickSuggest.sys.mjs`.
 *
 * [1] Typically a feature should manage only one type. In rare cases, it might
 * make sense to manage multiple types, for example when a single Merino
 * provider serves more than one type of suggestion.
 */
export class SuggestProvider extends SuggestFeature {
  // Methods designed for overriding below

  /**
   * @returns {string}
   *   If the feature's suggestions are served by Merino, the subclass should
   *   override this getter and return the name of the Merino provider that
   *   serves them.
   */
  get merinoProvider() {
    return "";
  }

  /**
   * @returns {string}
   *   If the feature's suggestions are served by the Rust component, the
   *   subclass should override this getter and return their type name as
   *   defined by the `Suggestion` enum in the component. e.g., "Amp",
   *   "Wikipedia", "Mdn", etc.
   */
  get rustSuggestionType() {
    return "";
  }

  /**
   * @returns {object|null}
   *   If the feature manages suggestions served by the Rust component that
   *   require provider constraints, the subclass should override this getter
   *   and return a plain JS object that can be passed to
   *   `SuggestionProviderConstraints()`. This getter will only be called if the
   *   feature is enabled.
   */
  get rustProviderConstraints() {
    return null;
  }

  /**
   * @returns {string}
   *   If the feature's suggestions are served by the ML backend, the subclass
   *   should override this getter and return the ML intent name as returned by
   *   `MLSuggest`. e.g., "yelp_intent"
   */
  get mlIntent() {
    return "";
  }

  /**
   * @returns {boolean}
   *   If the feature's suggestions are served by the ML backend, the subclass
   *   should override this getter and return true if the ML suggestions should
   *   be enabled and false otherwise.
   */
  get isMlIntentEnabled() {
    return false;
  }

  /**
   * Subclasses should typically override this method. It should return the
   * telemetry type for the given suggestion. A telemetry type uniquely
   * identifies a type of Suggest suggestion independent of the backend that
   * returned it. It's used to build the result type values that are recorded in
   * urlbar telemetry. The telemetry type does not include the suggestion's
   * source/backend. For example, "adm_sponsored" is the AMP suggestion
   * telemetry type, not "rust_adm_sponsored".
   *
   * @param {object} suggestion
   *   A suggestion returned by one of the Suggest backends.
   * @returns {string}
   *   The suggestion's telemetry type.
   */
  getSuggestionTelemetryType(suggestion) {
    return this.merinoProvider;
  }

  /**
   * The subclass should override this method if it manages any sponsored
   * suggestion types. It should return true if the given suggestion should be
   * considered sponsored.
   *
   * @param {object} suggestion
   *   A suggestion returned by one of the Suggest backends.
   * @returns {boolean}
   *   Whether the suggestion should be considered sponsored.
   */
  isSuggestionSponsored(suggestion) {
    return false;
  }

  /**
   * The subclass may override this method as necessary. It will be called once
   * per query with all of the feature's suggestions that matched the query. It
   * should filter out suggestions that should not be shown. This is useful in
   * cases where a backend may return many of the feature's suggestions but only
   * some of them should be shown, and the criteria for determining which to
   * show are external to the backend.
   *
   * `makeResult()` can also be used to filter suggestions by returning null for
   * suggestions that should be discarded. Use `filterSuggestions()` when you
   * need to know all matching suggestions in order to decide which to show.
   *
   * @param {Array} suggestions
   *   All the feature's suggestions that matched a query.
   * @returns {Promise<Array>}
   *   The subset of `suggestions` that should be shown (typically all).
   */
  async filterSuggestions(suggestions) {
    return suggestions;
  }

  /**
   * The subclass should override this method. It should return either a new
   * `UrlbarResult` for the given suggestion or null if the suggestion should
   * not be shown.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {object} suggestion
   *   A suggestion returned by one of the Suggest backends.
   * @param {string} searchString
   *   The search string that was used to fetch the suggestion. It may be
   *   different from `queryContext.searchString` due to trimming, lower-casing,
   *   etc. This is included as a param in case it's useful.
   * @returns {Promise<UrlbarResult|null>}
   *   A new result for the suggestion or null if a result should not be shown.
   */
  async makeResult(queryContext, suggestion, searchString) {
    return null;
  }

  /**
   * The subclass may override this method as necessary. It's analogous to
   * `UrlbarProvider.onImpression()` and will be called when one or more of the
   * feature's results were visible at the end of a urlbar session.
   *
   * @param {string} state
   *   The user-interaction state. See `UrlbarProvider.onImpression()`.
   * @param {UrlbarQueryContext} queryContext
   *   The urlbar session's query context.
   * @param {UrlbarController} controller
   *   The controller.
   * @param {Array} featureResults
   *   The feature's results that were visible at the end of the session. This
   *   will always be non-empty and will only contain results from the feature.
   * @param {object|null} details
   *   Details about the engagement. See `UrlbarProvider.onImpression()`.
   */
  onImpression(state, queryContext, controller, featureResults, details) {}

  /**
   * The subclass may override this method as necessary. It's analogous to
   * `UrlbarProvider.onEngagement()` and will be called when the user engages
   * with a result from the feature.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The urlbar session's query context.
   * @param {UrlbarController} controller
   *   The controller.
   * @param {object|null} details
   *   See `UrlbarProvider.onEngagement()`.
   * @param {string} searchString
   *   The actual search string used to fetch Suggest results. It might be
   *   slightly different from `queryContext.searchString`. e.g., it might be
   *   trimmed differently.
   */
  onEngagement(queryContext, controller, details, searchString) {}

  /**
   * Some features may create result URLs that are potentially unique per query.
   * Typically this is done by modifying an original suggestion URL at query
   * time, for example by adding timestamps or query-specific search params. In
   * that case, a single original suggestion URL will map to many result URLs.
   * If this is true for the subclass, it should override this method and return
   * whether the given URL and result URL both map back to the same original
   * suggestion URL.
   *
   * @param {string} url
   *   The URL to check, typically from the user's history.
   * @param {UrlbarResult} result
   *   The Suggest result.
   * @returns {boolean}
   *   Whether `url` is equivalent to the result's URL.
   */
  isUrlEquivalentToResultUrl(url, result) {
    return url == result.payload.url;
  }

  // Methods not designed for overriding below

  /**
   * Enables or disables the feature. If the feature manages any Rust suggestion
   * types that become enabled as a result, they will be ingested.
   */
  update() {
    super.update();
    lazy.QuickSuggest.rustBackend?.ingestEnabledSuggestions(this);
  }
}

/**
 * Base class for Suggest features that serve suggestions. None of the methods
 * will be called when the backend is disabled.
 *
 * Subclasses should be registered with `QuickSuggest` by adding them to the
 * `FEATURES` const in `QuickSuggest.sys.mjs`.
 */
export class SuggestBackend extends SuggestFeature {
  // Methods designed for overriding below

  /**
   * The subclass should override this method. It should fetch and return
   * matching suggestions.
   *
   * @param {string} searchString
   *   The search string.
   * @param {object} options
   *   Options object.
   * @param {UrlbarQueryContext} options.queryContext
   *   The query context.
   * @param {Array} options.types
   *   This is only intended to be used in special circumstances and normally
   *   should not be specified. Array of suggestion types to query. By default
   *   all enabled suggestion types are queried.
   * @returns {Promise<Array>}
   *   Array of matching suggestions. An empty array should be returned if no
   *   suggestions matched or suggestions can't be fetched for any reason.
   * @abstract
   */
  async query(searchString, { queryContext, types }) {
    throw new Error("Trying to access the base class, must be overridden");
  }

  /**
   * The subclass should override this method if anything needs to be stopped or
   * cleaned up when a query is canceled.
   */
  cancelQuery() {}

  /**
   * The subclass should override this method as necessary. It's called on the
   * backend in response to `UrlbarProviderQuickSuggest.onSearchSessionEnd()`.
   *
   * @param {UrlbarQueryContext} queryContext
   *    The query context.
   * @param {UrlbarController} controller
   *    The controller.
   * @param {object} details
   *    Details object.
   */
  onSearchSessionEnd(queryContext, controller, details) {}
}
