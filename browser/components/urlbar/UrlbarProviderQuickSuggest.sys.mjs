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
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarProviderSearchSuggestions:
    "resource:///modules/UrlbarProviderSearchSuggestions.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarSearchUtils: "resource:///modules/UrlbarSearchUtils.sys.mjs",
});

// `contextId` is a unique identifier used by Contextual Services
const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
ChromeUtils.defineLazyGetter(lazy, "contextId", () => {
  let _contextId = Services.prefs.getStringPref(CONTEXT_ID_PREF, null);
  if (!_contextId) {
    _contextId = String(Services.uuid.generateUUID());
    Services.prefs.setStringPref(CONTEXT_ID_PREF, _contextId);
  }
  return _contextId;
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

    // Fetch suggestions from all enabled sources.
    let promises = [];
    if (lazy.QuickSuggest.rustBackend?.isEnabled) {
      promises.push(lazy.QuickSuggest.rustBackend.query(searchString));
    }
    if (
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled") &&
      queryContext.allowRemoteResults()
    ) {
      promises.push(this._fetchMerinoSuggestions(queryContext, searchString));
    }
    let mlBackend = lazy.QuickSuggest.getFeature("SuggestBackendMl");
    if (mlBackend.isEnabled) {
      promises.push(mlBackend.query(queryContext.trimmedLowerCaseSearchString));
    }

    // Wait for both sources to finish.
    let values = await Promise.all(promises);
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

      let canAdd = await this._canAddSuggestion(suggestion);
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

      // Ensure all suggestions have scores. `quickSuggestScoreMap`, if defined,
      // maps telemetry types to score overrides.
      if (isNaN(suggestion.score)) {
        suggestion.score = DEFAULT_SUGGESTION_SCORE;
      }
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

  onImpression(state, queryContext, controller, providerVisibleResults) {
    // Legacy Suggest telemetry should be recorded when a Suggest result is
    // visible at the end of an engagement on any result.
    this.#sessionResult =
      state == "engagement" ? providerVisibleResults[0].result : null;
  }

  onEngagement(queryContext, controller, details) {
    if (details.isSessionOngoing) {
      // When the session remains ongoing -- e.g., a result is dismissed --
      // tests expect legacy telemetry to be recorded immediately on engagement,
      // not deferred until the session ends, so record it now.
      this.#recordEngagement(queryContext, details.result, details);
    }

    let feature = this.#getFeatureByResult(details.result);
    if (feature?.handleCommand) {
      feature.handleCommand(
        controller.view,
        details.result,
        details.selType,
        this._trimmedSearchString
      );
    } else if (details.selType == "dismiss") {
      // Handle dismissals.
      this.#dismissResult(controller, details.result);
    }

    feature?.onEngagement?.(queryContext, controller, details);
  }

  onSearchSessionEnd(queryContext, controller, details) {
    // Reset the Merino session ID when a session ends. By design for the user's
    // privacy, we don't keep it around between engagements.
    this.#merino?.resetSession();

    // Record legacy Suggest telemetry.
    this.#recordEngagement(queryContext, this.#sessionResult, details);

    this.#sessionResult = null;
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
   *   A suggestion from remote settings or Merino.
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

  #dismissResult(controller, result) {
    if (!result.payload.isBlockable) {
      this.logger.info("Dismissals disabled, ignoring dismissal");
      return;
    }

    this.logger.info("Dismissing result", result);
    lazy.QuickSuggest.blockedSuggestions.add(
      // adM results have `originalUrl`, which contains timestamp templates.
      result.payload.originalUrl ?? result.payload.url
    );
    controller.removeResult(result);
  }

  /**
   * Records engagement telemetry. This should be called only at the end of an
   * engagement when a quick suggest result is present or when a quick suggest
   * result is dismissed.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {UrlbarResult} result
   *   The quick suggest result that was present (and possibly picked) at the
   *   end of the engagement or that was dismissed. Null if no quick suggest
   *   result was present.
   * @param {object} details
   *   The `details` object that was passed to `onEngagement()` or
   *   `onSearchSessionEnd()`.
   */
  #recordEngagement(queryContext, result, details) {
    let resultSelType = "";
    let resultClicked = false;
    if (result && details.result == result) {
      resultSelType = details.selType;
      resultClicked =
        details.element?.tagName != "menuitem" &&
        !details.element?.classList.contains("urlbarView-button") &&
        details.selType != "dismiss";
    }

    if (result) {
      // Update impression stats.
      lazy.QuickSuggest.impressionCaps.updateStats(
        result.payload.isSponsored ? "sponsored" : "nonsponsored"
      );

      // Record engagement pings.
      if (!queryContext.isPrivate) {
        this.#recordEngagementPings({ result, resultSelType, resultClicked });
      }
    }
  }

  /**
   * Helper for engagement telemetry that records custom contextual services
   * pings.
   *
   * @param {object} options
   *   Options object
   * @param {UrlbarResult} options.result
   *   The quick suggest result related to the engagement. Must not be null.
   * @param {string} options.resultSelType
   *   If an element in the result's row was clicked, this should be its
   *   `selType`. Otherwise it should be an empty string.
   * @param {boolean} options.resultClicked
   *   True if the main part of the result's row was clicked; false if a button
   *   like help or dismiss was clicked or if no part of the row was clicked.
   */
  #recordEngagementPings({ result, resultSelType, resultClicked }) {
    if (
      result.payload.telemetryType != "adm_sponsored" &&
      result.payload.telemetryType != "adm_nonsponsored"
    ) {
      return;
    }

    // Contextual services ping paylod
    let payload = {
      match_type: result.isBestMatch ? "best-match" : "firefox-suggest",
      // Always use lowercase to make the reporting consistent
      advertiser: result.payload.sponsoredAdvertiser.toLocaleLowerCase(),
      block_id: result.payload.sponsoredBlockId,
      improve_suggest_experience_checked: lazy.UrlbarPrefs.get(
        "quicksuggest.dataCollection.enabled"
      ),
      // Quick suggest telemetry indexes are 1-based but `rowIndex` is 0-based
      position: result.rowIndex + 1,
      suggested_index: result.suggestedIndex,
      suggested_index_relative_to_group:
        !!result.isSuggestedIndexRelativeToGroup,
      request_id: result.payload.requestId,
      source: result.payload.source,
    };

    // Glean ping key -> value
    let defaultValuesByGleanKey = {
      matchType: payload.match_type,
      advertiser: payload.advertiser,
      blockId: payload.block_id,
      improveSuggestExperience: payload.improve_suggest_experience_checked,
      position: payload.position,
      suggestedIndex: payload.suggested_index.toString(),
      suggestedIndexRelativeToGroup: payload.suggested_index_relative_to_group,
      requestId: payload.request_id,
      source: payload.source,
      contextId: lazy.contextId,
    };

    let sendGleanPing = valuesByGleanKey => {
      valuesByGleanKey = { ...defaultValuesByGleanKey, ...valuesByGleanKey };
      for (let [gleanKey, value] of Object.entries(valuesByGleanKey)) {
        let glean = Glean.quickSuggest[gleanKey];
        if (value !== undefined && value !== "") {
          glean.set(value);
        }
      }
      GleanPings.quickSuggest.submit();
    };

    // impression
    sendGleanPing({
      pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
      isClicked: resultClicked,
      reportingUrl: result.payload.sponsoredImpressionUrl,
    });

    // click
    if (resultClicked) {
      sendGleanPing({
        pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
        reportingUrl: result.payload.sponsoredClickUrl,
      });
    }

    // dismiss
    if (resultSelType == "dismiss") {
      sendGleanPing({
        pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
        iabCategory: result.payload.sponsoredIabCategory,
      });
    }
  }

  /**
   * Cancels the current query.
   */
  cancelQuery() {
    // Cancel the Rust query.
    lazy.QuickSuggest.rustBackend?.cancelQuery();

    // Cancel the Merino timeout timer so it doesn't fire and record a timeout.
    // If it's already canceled or has fired, this is a no-op.
    this.#merino?.cancelTimeoutTimer();

    // Don't abort the Merino fetch if one is ongoing. By design we allow
    // fetches to finish so we can record their latency.
  }

  /**
   * Fetches Merino suggestions.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context.
   * @param {string} searchString
   *   The search string.
   * @returns {Array}
   *   The Merino suggestions or null if there's an error or unexpected
   *   response.
   */
  async _fetchMerinoSuggestions(queryContext, searchString) {
    if (!this.#merino) {
      this.#merino = new lazy.MerinoClient(this.name);
    }

    let providers;
    if (
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") &&
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      !lazy.UrlbarPrefs.get("merinoProviders")
    ) {
      // Data collection is enabled but suggestions are not. Use an empty list
      // of providers to tell Merino not to fetch any suggestions.
      providers = [];
    }

    let suggestions = await this.#merino.fetch({
      providers,
      query: searchString,
    });

    await this.#applyRanking(suggestions);

    return suggestions;
  }

  /**
   * Apply ranking to suggestions by updating their scores.
   *
   * @param {Array} suggestions
   *   The suggestions to be ranked.
   */
  async #applyRanking(suggestions) {
    switch (lazy.UrlbarPrefs.get("quickSuggestRankingMode")) {
      case "random":
        this.#updateScoreRandomly(suggestions);
        break;
      case "interest":
        await this.#updateScorebyRelevance(suggestions);
        break;
      case "default":
      default:
        // Do nothing.
        break;
    }
  }

  /**
   * Only exposed for testing.
   *
   * @param {Array} suggestions
   *   The suggestions to be ranked.
   */
  async _test_applyRanking(suggestions) {
    await this.#applyRanking(suggestions);
  }

  /**
   * Update score by randomly selecting a winner and boosting its score with
   * the highest score among the candidates plus a small addition.
   *
   * @param {Array} suggestions
   *   The suggestions to be ranked.
   */
  #updateScoreRandomly(suggestions) {
    if (suggestions.length <= 1) {
      return;
    }

    const winner = suggestions[Math.floor(Math.random() * suggestions.length)];
    const oldScore = winner.score;
    const highest = Math.max(
      ...suggestions.map(suggestion => suggestion.score || 0)
    );
    winner.score = highest + 0.001;

    this.logger.debug("Updated suggestion score (randomly)", {
      oldScore,
      newScore: winner.score.toFixed(3),
    });
  }

  /**
   * Update score by interest-based relevance scoring. The final score is a mean
   * between the interest-based score and the default static score, which means
   * if the former is 0 or less than the latter, the combined score will be less
   * than the static score.
   *
   * @param {Array} suggestions
   *   The suggestions to be ranked.
   */
  async #updateScorebyRelevance(suggestions) {
    for (let suggestion of suggestions) {
      if (suggestion.categories?.length) {
        try {
          let score = await lazy.ContentRelevancyManager.score(
            suggestion.categories,
            true // adjustment needed b/c Merino uses the original encoding
          );
          Glean.suggestRelevance.status.success.add(1);
          let oldScore = suggestion.score;
          if (isNaN(oldScore)) {
            oldScore = DEFAULT_SUGGESTION_SCORE;
          }
          suggestion.score = (oldScore + score) / 2;
          Glean.suggestRelevance.outcome[
            suggestion.score >= oldScore ? "boosted" : "decreased"
          ].add(1);
          this.logger.debug("Updated suggestion score (by relevance)", {
            oldScore,
            newScore: suggestion.score.toFixed(2),
          });
        } catch (error) {
          Glean.suggestRelevance.status.failure.add(1);
          this.logger.error("Error updating suggestion score", error);
          continue;
        }
      }
    }
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
  async _canAddSuggestion(suggestion) {
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

  get _test_merino() {
    return this.#merino;
  }

  // The result from this provider that was visible at the end of the current
  // search session, if the session ended in an engagement.
  #sessionResult;

  // The Merino client.
  #merino = null;
}

export var UrlbarProviderQuickSuggest = new ProviderQuickSuggest();
