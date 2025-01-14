/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContentRelevancyManager:
    "resource://gre/modules/ContentRelevancyManager.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderSearchSuggestions:
    "resource:///modules/UrlbarProviderSearchSuggestions.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

// Used for suggestions that don't otherwise have a score.
const DEFAULT_SUGGESTION_SCORE = 0.2;

/**
 * A provider that returns a suggested url to the user based on what
 * they have currently typed so they can navigate directly.
 */
class ProviderQuickSuggest extends UrlbarProvider {
  /**
   * Returns the name of this provider.
   *
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UrlbarProviderQuickSuggest";
  }

  /**
   * The type of the provider.
   *
   * @returns {UrlbarUtils.PROVIDER_TYPE}
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.NETWORK;
  }

  /**
   * @returns {number}
   *   The default score for suggestions that don't otherwise have one. All
   *   suggestions require scores so they can be ranked. Scores are numeric
   *   values in the range [0, 1].
   */
  get DEFAULT_SUGGESTION_SCORE() {
    return DEFAULT_SUGGESTION_SCORE;
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
    // If the sources don't include search or the user used a restriction
    // character other than search, don't allow any suggestions.
    if (
      !queryContext.sources.includes(UrlbarUtils.RESULT_SOURCE.SEARCH) ||
      (queryContext.restrictSource &&
        queryContext.restrictSource != UrlbarUtils.RESULT_SOURCE.SEARCH)
    ) {
      return false;
    }

    if (
      !lazy.UrlbarPrefs.get("quickSuggestEnabled") ||
      queryContext.isPrivate ||
      queryContext.searchMode
    ) {
      return false;
    }

    // Trim only the start of the search string because a trailing space can
    // affect the suggestions.
    let trimmedSearchString = queryContext.searchString.trimStart();

    // Per product requirements, at least two characters must be typed to
    // trigger a Suggest suggestion. Suggestion keywords should always be at
    // least two characters long, but we check here anyway to be safe. Note we
    // called `trimStart()` above, so we only call `trimEnd()` here.
    if (trimmedSearchString.trimEnd().length < 2) {
      return false;
    }
    this._trimmedSearchString = trimmedSearchString;
    return true;
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
    let instance = this.queryInstance;
    let searchString = this._trimmedSearchString;

    // Fetch suggestions from all enabled backends.
    let values = await Promise.all(
      lazy.QuickSuggest.enabledBackends.map(backend =>
        backend.query(searchString, { queryContext })
      )
    );
    if (instance != this.queryInstance) {
      return;
    }

    let suggestions = await this.#filterAndSortSuggestions(values.flat());
    if (instance != this.queryInstance) {
      return;
    }

    // Convert each suggestion into a result and add it. Don't add more than
    // `maxResults` visible results so we don't spam the muxer.
    let remainingCount = queryContext.maxResults ?? 10;
    for (let suggestion of suggestions) {
      if (!remainingCount) {
        break;
      }

      let canAdd = await this.#canAddSuggestion(suggestion);
      if (instance != this.queryInstance) {
        return;
      }
      if (canAdd) {
        let result = await this.#makeResult(queryContext, suggestion);
        if (instance != this.queryInstance) {
          return;
        }
        if (result) {
          addCallback(this, result);
          if (!result.isHiddenExposure) {
            remainingCount--;
          }
        }
      }
    }
  }

  async #filterAndSortSuggestions(suggestions) {
    let requiredKeys = ["source", "provider"];
    let scoreMap = lazy.UrlbarPrefs.get("quickSuggestScoreMap");
    let suggestionsByFeature = new Map();
    let indexesBySuggestion = new Map();

    for (let i = 0; i < suggestions.length; i++) {
      let suggestion = suggestions[i];

      // Discard suggestions that don't have the required keys, which are used
      // to look up their features. Normally this shouldn't ever happen.
      if (!requiredKeys.every(key => suggestion[key])) {
        this.logger.error("Suggestion is missing one or more required keys", {
          requiredKeys,
          suggestion,
        });
        continue;
      }

      // Set `is_sponsored` before continuing because
      // `#getSuggestionTelemetryType()` and other things depend on it.
      let feature = this.#getFeature(suggestion);
      suggestion.is_sponsored = !!feature?.isSuggestionSponsored(suggestion);

      // Ensure all suggestions have scores.
      //
      // Step 1: Set a default score if the suggestion doesn't have one.
      if (typeof suggestion.score != "number" || isNaN(suggestion.score)) {
        suggestion.score = DEFAULT_SUGGESTION_SCORE;
      }

      // Step 2: Apply relevancy ranking. For now we only do this for Merino
      // suggestions, but we may expand it in the future.
      if (suggestion.source == "merino") {
        await this.#applyRanking(suggestion);
      }

      // Step 3: Apply score overrides defined in `quickSuggestScoreMap`. It
      // maps telemetry types to scores.
      if (scoreMap) {
        let telemetryType = this.#getSuggestionTelemetryType(suggestion);
        if (scoreMap.hasOwnProperty(telemetryType)) {
          let score = parseFloat(scoreMap[telemetryType]);
          if (!isNaN(score)) {
            suggestion.score = score;
          }
        }
      }

      // Save some state used below to build the final list of suggestions.
      let featureSuggestions = suggestionsByFeature.get(feature);
      if (!featureSuggestions) {
        featureSuggestions = [];
        suggestionsByFeature.set(feature, featureSuggestions);
      }
      featureSuggestions.push(suggestion);
      indexesBySuggestion.set(suggestion, i);
    }

    // Let each feature filter its suggestions.
    suggestions = (
      await Promise.all(
        [...suggestionsByFeature].map(([feature, featureSuggestions]) =>
          feature
            ? feature.filterSuggestions(featureSuggestions)
            : Promise.resolve(featureSuggestions)
        )
      )
    ).flat();

    // Sort the suggestions. When scores are equal, sort by original index to
    // ensure a stable sort.
    suggestions.sort((a, b) => {
      return (
        b.score - a.score ||
        indexesBySuggestion.get(a) - indexesBySuggestion.get(b)
      );
    });

    return suggestions;
  }

  onImpression(state, queryContext, controller, resultsAndIndexes, details) {
    // Build a map from each feature to its results in `resultsAndIndexes`.
    let resultsByFeature = resultsAndIndexes.reduce((memo, { result }) => {
      let feature = this.#getFeatureByResult(result);
      if (feature) {
        let featureResults = memo.get(feature);
        if (!featureResults) {
          featureResults = [];
          memo.set(feature, featureResults);
        }
        featureResults.push(result);
      }
      return memo;
    }, new Map());

    // Notify each feature with its results.
    for (let [feature, featureResults] of resultsByFeature) {
      feature.onImpression(
        state,
        queryContext,
        controller,
        featureResults,
        details
      );
    }
  }

  onEngagement(queryContext, controller, details) {
    let feature = this.#getFeatureByResult(details.result);
    feature?.onEngagement(
      queryContext,
      controller,
      details,
      this._trimmedSearchString
    );
  }

  onSearchSessionEnd(queryContext, controller, details) {
    for (let backend of lazy.QuickSuggest.enabledBackends) {
      backend.onSearchSessionEnd(queryContext, controller, details);
    }
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result) {
    return this.#getFeatureByResult(result)?.getViewUpdate?.(result);
  }

  getResultCommands(result) {
    return this.#getFeatureByResult(result)?.getResultCommands?.(result);
  }

  /**
   * Gets the `BaseFeature` instance that implements suggestions for a source
   * and provider name. The source and provider name can be supplied from either
   * a suggestion object or the payload of a `UrlbarResult` object.
   *
   * @param {object} options
   *   Options object.
   * @param {string} options.source
   *   The suggestion source, one of: "merino", "ml", "rust"
   * @param {string} options.provider
   *   This value depends on `source`. The possible values per source are:
   *
   *   merino:
   *     The name of the Merino provider that serves the suggestion type
   *   ml:
   *     The name of the intent as determined by `MLSuggest`
   *   rust:
   *     The name of the suggestion type as defined in Rust
   * @returns {BaseFeature}
   *   The feature instance or null if no feature was found.
   */
  #getFeature({ source, provider }) {
    switch (source) {
      case "merino":
        return lazy.QuickSuggest.getFeatureByMerinoProvider(provider);
      case "rust":
        return lazy.QuickSuggest.getFeatureByRustSuggestionType(provider);
      case "ml":
        return lazy.QuickSuggest.getFeatureByMlIntent(provider);
    }
    return null;
  }

  #getFeatureByResult(result) {
    return this.#getFeature(result.payload);
  }

  /**
   * Returns the telemetry type for a suggestion. A telemetry type uniquely
   * identifies a type of suggestion as well as the kind of `UrlbarResult`
   * instances created from it.
   *
   * @param {object} suggestion
   *   A suggestion from a Suggest backend.
   * @returns {string}
   *   The telemetry type. If the suggestion type is managed by a `BaseFeature`
   *   instance, the telemetry type is retrieved from it. Otherwise the
   *   suggestion type is assumed to come from Merino, and `suggestion.provider`
   *   (the Merino provider name) is returned.
   */
  #getSuggestionTelemetryType(suggestion) {
    let feature = this.#getFeature(suggestion);
    if (feature) {
      return feature.getSuggestionTelemetryType(suggestion);
    }
    return suggestion.provider;
  }

  async #makeResult(queryContext, suggestion) {
    let result;
    let feature = this.#getFeature(suggestion);
    if (!feature) {
      // We specifically allow Merino to serve suggestion types that Firefox
      // doesn't know about so that we can experiment with new types without
      // requiring changes in Firefox. No other source should return unknown
      // suggestion types with the possible exception of the ML backend: Its
      // models are stored in remote settings and it may return newer intents
      // that aren't recognized by older Firefoxes.
      if (suggestion.source != "merino") {
        return null;
      }
      result = this.#makeDefaultResult(queryContext, suggestion);
    } else {
      result = await feature.makeResult(
        queryContext,
        suggestion,
        this._trimmedSearchString
      );
      if (!result) {
        // Feature might return null, if the feature is disabled and so on.
        return null;
      }
    }

    // See `#getFeature()` for possible values of `source` and `provider`.
    result.payload.source = suggestion.source;
    result.payload.provider = suggestion.provider;
    result.payload.telemetryType = this.#getSuggestionTelemetryType(suggestion);

    // Handle icons here so each feature doesn't have to do it, but use `||=` to
    // let them do it if they need to.
    result.payload.icon ||= suggestion.icon;
    result.payload.iconBlob ||= suggestion.icon_blob;

    // Set the appropriate suggested index and related properties unless the
    // feature did it already.
    if (!result.hasSuggestedIndex) {
      if (suggestion.is_top_pick) {
        result.isBestMatch = true;
        result.isRichSuggestion = true;
        result.richSuggestionIconSize ||= 52;
        result.suggestedIndex = 1;
      } else if (
        !isNaN(suggestion.position) &&
        lazy.UrlbarPrefs.get("quickSuggestAllowPositionInSuggestions")
      ) {
        result.suggestedIndex = suggestion.position;
      } else {
        result.isSuggestedIndexRelativeToGroup = true;
        if (!suggestion.is_sponsored) {
          result.suggestedIndex = lazy.UrlbarPrefs.get(
            "quickSuggestNonSponsoredIndex"
          );
        } else if (
          lazy.UrlbarPrefs.get("showSearchSuggestionsFirst") &&
          lazy.UrlbarProviderSearchSuggestions.isActive(queryContext) &&
          lazy.UrlbarSearchUtils.getDefaultEngine(
            queryContext.isPrivate
          ).supportsResponseType(lazy.SearchUtils.URL_TYPE.SUGGEST_JSON)
        ) {
          // Show sponsored suggestions somewhere other than the bottom of the
          // Suggest section only if search suggestions are shown first, the
          // search suggestions provider is active for the current context (it
          // will not be active if search suggestions are disabled, among other
          // reasons), and the default engine supports suggestions.
          result.suggestedIndex = lazy.UrlbarPrefs.get(
            "quickSuggestSponsoredIndex"
          );
        } else {
          result.suggestedIndex = -1;
        }
      }
    }

    return result;
  }

  #makeDefaultResult(queryContext, suggestion) {
    let payload = {
      url: suggestion.url,
      isSponsored: suggestion.is_sponsored,
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      isManageable: true,
    };

    if (suggestion.full_keyword) {
      payload.title = suggestion.title;
      payload.qsSuggestion = [
        suggestion.full_keyword,
        UrlbarUtils.HIGHLIGHT.SUGGESTED,
      ];
    } else {
      payload.title = [suggestion.title, UrlbarUtils.HIGHLIGHT.TYPED];
      payload.shouldShowUrl = true;
    }

    return new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );
  }

  /**
   * Cancels the current query.
   */
  cancelQuery() {
    for (let backend of lazy.QuickSuggest.enabledBackends) {
      backend.cancelQuery();
    }
  }

  /**
   * Applies relevancy ranking to a suggestion by updating its score.
   *
   * @param {object} suggestion
   *   The suggestion to be ranked.
   */
  async #applyRanking(suggestion) {
    let oldScore = suggestion.score;

    let mode = lazy.UrlbarPrefs.get("quickSuggestRankingMode");
    switch (mode) {
      case "random":
        suggestion.score = Math.random();
        break;
      case "interest":
        await this.#updateScoreByRelevance(suggestion);
        break;
      case "default":
      default:
        // Do nothing.
        return;
    }

    this.logger.debug("Applied ranking to suggestion score", {
      mode,
      oldScore,
      newScore: suggestion.score.toFixed(3),
    });
  }

  /**
   * Update score by interest-based relevance scoring. The final score is a mean
   * between the interest-based score and the default static score, which means
   * if the former is 0 or less than the latter, the combined score will be less
   * than the static score.
   *
   * @param {object} suggestion
   *   The suggestion to be ranked.
   */
  async #updateScoreByRelevance(suggestion) {
    if (!suggestion.categories?.length) {
      return;
    }

    let score;
    try {
      score = await lazy.ContentRelevancyManager.score(
        suggestion.categories,
        true // adjustment needed b/c Merino uses the original encoding
      );
    } catch (error) {
      Glean.suggestRelevance.status.failure.add(1);
      this.logger.error("Error updating suggestion score", error);
      return;
    }

    Glean.suggestRelevance.status.success.add(1);
    let oldScore = suggestion.score;
    suggestion.score = (oldScore + score) / 2;
    Glean.suggestRelevance.outcome[
      suggestion.score >= oldScore ? "boosted" : "decreased"
    ].add(1);
  }

  /**
   * Returns whether a given suggestion can be added for a query, assuming the
   * provider itself should be active.
   *
   * @param {object} suggestion
   *   The suggestion to check.
   * @returns {boolean}
   *   Whether the suggestion can be added.
   */
  async #canAddSuggestion(suggestion) {
    this.logger.debug("Checking if suggestion can be added", suggestion);

    // Return false if suggestions are disabled. Always allow Rust exposure
    // suggestions.
    if (
      ((suggestion.is_sponsored &&
        !lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored")) ||
        (!suggestion.is_sponsored &&
          !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored"))) &&
      (suggestion.source != "rust" || suggestion.provider != "Exposure")
    ) {
      this.logger.debug("Suggestions disabled, not adding suggestion");
      return false;
    }

    // Return false if an impression cap has been hit.
    if (
      (suggestion.is_sponsored &&
        lazy.UrlbarPrefs.get("quickSuggestImpressionCapsSponsoredEnabled")) ||
      (!suggestion.is_sponsored &&
        lazy.UrlbarPrefs.get("quickSuggestImpressionCapsNonSponsoredEnabled"))
    ) {
      let type = suggestion.is_sponsored ? "sponsored" : "nonsponsored";
      let hitStats = lazy.QuickSuggest.impressionCaps.getHitStats(type);
      if (hitStats) {
        this.logger.debug("Impression cap(s) hit, not adding suggestion", {
          type,
          hitStats,
        });
        return false;
      }
    }

    // Return false if the suggestion is blocked based on its URL. Suggestions
    // from the JS backend define a single `url` property. Suggestions from the
    // Rust backend are more complicated: Sponsored suggestions define `rawUrl`,
    // which may contain timestamp templates, while non-sponsored suggestions
    // define only `url`. Blocking should always be based on URLs with timestamp
    // templates, where applicable, so check `rawUrl` and then `url`, in that
    // order.
    let { blockedSuggestions } = lazy.QuickSuggest;
    if (await blockedSuggestions.has(suggestion.rawUrl ?? suggestion.url)) {
      this.logger.debug("Suggestion blocked, not adding suggestion");
      return false;
    }

    this.logger.debug("Suggestion can be added");
    return true;
  }

  async _test_applyRanking(suggestion) {
    await this.#applyRanking(suggestion);
  }
}

export var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
