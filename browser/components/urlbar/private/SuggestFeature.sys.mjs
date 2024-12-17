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
   * {boolean}
   *   Whether the feature should be enabled. Typically the subclass will check
   *   the values of one or more Nimbus variables or preferences. `QuickSuggest`
   *   will access this getter only when Suggest is enabled. When Suggest is
   *   disabled, the feature will be disabled automatically.
   */
  get shouldEnable() {
    throw new Error("`shouldEnable` must be overridden");
  }

  /**
   * @returns {Array}
   *   If the subclass's `shouldEnable` implementation depends on any prefs that
   *   are not fallbacks for Nimbus variables, the subclass should override this
   *   getter and return their names in this array so that `update()` can be
   *   called when they change. Names should be recognized by `UrlbarPrefs`. It
   *   doesn't hurt to include prefs that are fallbacks for Nimbus variables,
   *   it's just not necessary because `QuickSuggest` will update all features
   *   whenever a `urlbar` Nimbus variable or its fallback pref changes.
   */
  get enablingPreferences() {
    return null;
  }

  /**
   * This method should initialize or uninitialize any state related to the
   * feature.
   *
   * @param {boolean} enabled
   *   Whether the feature should be enabled or not.
   */
  enable(enabled) {}

  // Methods not designed for overriding below

  /**
   * @returns {Logger}
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
   * called with the new status; otherwise `enable()` is not called. If the
   * feature manages any Rust suggestion types that become enabled as a result,
   * they will be ingested.
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
 * Base class for Suggest features that manage one or more suggestion types.
 * Typically a feature should manage only one type, but it's possible to manage
 * more for any reason, usually in these cases:
 *
 * - When a single suggestion type served by a backend maps to more than one
 *   kind of `UrlbarResult`, for example when a Merino provider serves a single
 *   type that maps to many types on the client
 * - For historical reasons features can manage more than one Rust suggestion
 *   type by returning multiple values in the `rustSuggestionTypes` array
 *
 * The same suggestion type can be served by multiple backends, and a single
 * `SuggestProvider` subclass can manage the type regardless of backend by
 * overriding the appropriate methods and getters.
 *
 * Subclasses should be registered with `QuickSuggest` by adding them to the
 * `FEATURES` const in `QuickSuggest.sys.mjs`.
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
   * @returns {Array}
   *   If the feature's suggestions are served by the Rust component, the
   *   subclass should override this getter and return an array of their type
   *   names as defined by the `Suggestion` enum in the component. e.g., "Amp",
   *   "Wikipedia", "Mdn", etc.
   */
  get rustSuggestionTypes() {
    return [];
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
   * If the feature manages one or more suggestion types served by the Suggest
   * Rust component, this method should return true if the given suggestion type
   * is enabled and false otherwise. Many features do nothing but manage a
   * single Rust suggestion type, and the suggestion type should be enabled iff
   * the feature itself is enabled. Those features can rely on the default
   * implementation here since a feature's Rust suggestions will not be fetched
   * if the feature is disabled. Other features either manage multiple
   * suggestion types or have functionality beyond their Rust suggestions and
   * need to remain enabled even when their suggestions are not. Those features
   * should override this method.
   *
   * @param {string} type
   *   A suggestion type name as defined by the `Suggestion` enum in the Rust
   *   component, e.g., "Amp", "Wikipedia", "Mdn", etc.
   * @returns {boolean}
   *   Whether the suggestion type is enabled.
   */
  isRustSuggestionTypeEnabled(type) {
    return true;
  }

  /**
   * If the feature manages suggestions served by the Suggest Rust component and
   * at least one of its suggestion providers requires constraints, the subclass
   * should override this method and return a plain JS object that can be passed
   * to `SuggestionProviderConstraints()`. This method will only be called if
   * the feature and suggestion type are enabled.
   *
   * @param {string} type
   *   A suggestion type name as defined by the `Suggestion` enum in the Rust
   *   component, e.g., "Amp", "Wikipedia", "Mdn", etc.
   * @returns {object|null}
   *   If the given type's provider requires constraints, this should return a
   *   plain JS object that can be passed to `SuggestionProviderConstraints()`.
   *   Otherwise it should return null.
   */
  getRustProviderConstraints(type) {
    return null;
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
   * @returns {Array}
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
   * @returns {UrlbarResult|null}
   *   A new result for the suggestion or null if a result should not be shown.
   */
  async makeResult(queryContext, suggestion, searchString) {
    return null;
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
   * @returns {Array}
   *   Array of matching suggestions. An empty array should be returned if no
   *   suggestions matched or suggestions can't be fetched for any reason.
   */
  async query(searchString, { queryContext }) {}

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
