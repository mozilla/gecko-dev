/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

const RESULT_MENU_COMMAND = {
  INACCURATE_LOCATION: "inaccurate_location",
  MANAGE: "manage",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

/**
 * A feature for Yelp suggestions.
 */
export class YelpSuggestions extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      lazy.UrlbarPrefs.get("yelpFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.yelp")
    );
  }

  get enablingPreferences() {
    return ["suggest.quicksuggest.sponsored", "suggest.yelp"];
  }

  get rustSuggestionTypes() {
    return ["Yelp"];
  }

  get mlIntent() {
    return "yelp_intent";
  }

  get showLessFrequentlyCount() {
    const count = lazy.UrlbarPrefs.get("yelp.showLessFrequentlyCount") || 0;
    return Math.max(count, 0);
  }

  get canShowLessFrequently() {
    const cap =
      lazy.UrlbarPrefs.get("yelpShowLessFrequentlyCap") ||
      lazy.QuickSuggest.backend.config?.showLessFrequentlyCap ||
      0;
    return !cap || this.showLessFrequentlyCount < cap;
  }

  isSuggestionSponsored(_suggestion) {
    return true;
  }

  getSuggestionTelemetryType() {
    return "yelp";
  }

  enable(enabled) {
    if (!enabled) {
      this.#metadataCache = null;
    }
  }

  async makeResult(queryContext, suggestion, searchString) {
    if (suggestion.source == "ml") {
      suggestion = this.#convertMlSuggestion(suggestion);
      if (!suggestion) {
        return null;
      }
    }

    // If the user clicked "Show less frequently" at least once or if the
    // subject wasn't typed in full, then apply the min length threshold and
    // return null if the entire search string is too short.
    if (
      (this.showLessFrequentlyCount || !suggestion.subjectExactMatch) &&
      searchString.length < this.#minKeywordLength
    ) {
      return null;
    }

    suggestion.is_top_pick = lazy.UrlbarPrefs.get("yelpSuggestPriority");

    let url = new URL(suggestion.url);
    let title = suggestion.title;
    if (!url.searchParams.has(suggestion.locationParam)) {
      let city = await this.#fetchCity();

      // If we can't get city from Merino, rely on Yelp own.
      if (city) {
        url.searchParams.set(suggestion.locationParam, city);

        if (!suggestion.hasLocationSign) {
          title += " in";
        }

        title += ` ${city}`;
      }
    }

    url.searchParams.set("utm_medium", "partner");
    url.searchParams.set("utm_source", "mozilla");

    let resultProperties = {
      isRichSuggestion: true,
      showFeedbackMenu: true,
    };
    if (!suggestion.is_top_pick) {
      let suggestedIndex = lazy.UrlbarPrefs.get("yelpSuggestNonPriorityIndex");
      if (suggestedIndex !== null) {
        resultProperties.isSuggestedIndexRelativeToGroup = true;
        resultProperties.suggestedIndex = suggestedIndex;
      }
    }

    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.URL,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        ...lazy.UrlbarResult.payloadAndSimpleHighlights(queryContext.tokens, {
          url: url.toString(),
          originalUrl: suggestion.url,
          title: [title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
          bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
          iconBlob: suggestion.icon_blob,
        })
      ),
      resultProperties
    );
  }

  async filterSuggestions(suggestions) {
    // Return only Rust suggestions if ML is enabled and vice versa. That will
    // make it easier to understand which suggestions are being served as we're
    // developing this new ML feature. As long as the ML backend is enabled, we
    // can't control which intents it matches, so it might match Yelp even when
    // Yelp ML is disabled.
    if (!lazy.UrlbarPrefs.get("yelpMlEnabled")) {
      return suggestions.filter(s => s.source != "ml");
    }

    let mlSuggestions = suggestions.filter(s => s.source == "ml");
    if (mlSuggestions.length) {
      // Suggestions must have their intended scores after this method returns
      // because they're sorted after this, so set the score now. We defer
      // setting other properties until `makeResult()`.
      if (!this.#metadataCache) {
        this.#metadataCache = await this.#makeMetadataCache();
      }
      for (let s of mlSuggestions) {
        s.score = this.#metadataCache.score;
      }
    }
    return mlSuggestions;
  }

  getResultCommands() {
    let commands = [
      {
        name: RESULT_MENU_COMMAND.INACCURATE_LOCATION,
        l10n: {
          id: "firefox-suggest-weather-command-inaccurate-location",
        },
      },
    ];

    if (this.canShowLessFrequently) {
      commands.push({
        name: RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY,
        l10n: {
          id: "firefox-suggest-command-show-less-frequently",
        },
      });
    }

    commands.push(
      {
        l10n: {
          id: "firefox-suggest-command-dont-show-this",
        },
        children: [
          {
            name: RESULT_MENU_COMMAND.NOT_RELEVANT,
            l10n: {
              id: "firefox-suggest-command-not-relevant",
            },
          },
          {
            name: RESULT_MENU_COMMAND.NOT_INTERESTED,
            l10n: {
              id: "firefox-suggest-command-not-interested",
            },
          },
        ],
      },
      { name: "separator" },
      {
        name: RESULT_MENU_COMMAND.MANAGE,
        l10n: {
          id: "urlbar-result-menu-manage-firefox-suggest",
        },
      }
    );

    return commands;
  }

  handleCommand(view, result, selType, searchString) {
    switch (selType) {
      case RESULT_MENU_COMMAND.MANAGE:
        // "manage" is handled by UrlbarInput, no need to do anything here.
        break;
      case RESULT_MENU_COMMAND.INACCURATE_LOCATION:
        // Currently the only way we record this feedback is in the Glean
        // engagement event. As with all commands, it will be recorded with an
        // `engagement_type` value that is the command's name, in this case
        // `inaccurate_location`.
        view.acknowledgeFeedback(result);
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-one-yelp",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
        lazy.UrlbarPrefs.set("suggest.yelp", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all-yelp",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        this.incrementShowLessFrequentlyCount();
        if (!this.canShowLessFrequently) {
          view.invalidateResultMenuCommands();
        }
        lazy.UrlbarPrefs.set("yelp.minKeywordLength", searchString.length + 1);
        break;
    }
  }

  incrementShowLessFrequentlyCount() {
    if (this.canShowLessFrequently) {
      lazy.UrlbarPrefs.set(
        "yelp.showLessFrequentlyCount",
        this.showLessFrequentlyCount + 1
      );
    }
  }

  get #minKeywordLength() {
    // Use the pref value if it has a user value (which means the user clicked
    // "Show less frequently") or if there's no Nimbus value. Otherwise use the
    // Nimbus value. This lets us override the pref's default value using Nimbus
    // if necessary.
    let hasUserValue = Services.prefs.prefHasUserValue(
      "browser.urlbar.yelp.minKeywordLength"
    );
    let nimbusValue = lazy.UrlbarPrefs.get("yelpMinKeywordLength");
    let minLength =
      hasUserValue || nimbusValue === null
        ? lazy.UrlbarPrefs.get("yelp.minKeywordLength")
        : nimbusValue;
    return Math.max(minLength, 0);
  }

  async #fetchCity() {
    let geo = await lazy.QuickSuggest.geolocation();
    if (!geo) {
      return null;
    }
    let { city, region } = geo;
    return [city, region].filter(loc => !!loc).join(", ");
  }

  #convertMlSuggestion(ml) {
    if (!ml.location?.city && !ml.location?.state && !ml.subject) {
      return null;
    }

    let loc = [ml.location?.city, ml.location?.state]
      .filter(s => !!s)
      .join(", ");
    let title = [ml.subject, loc].filter(s => !!s).join(" in ");

    let url = new URL(this.#metadataCache.urlOrigin);
    url.pathname = this.#metadataCache.urlPathname;
    if (ml.subject) {
      url.searchParams.set(this.#metadataCache.findDesc, ml.subject);
    }
    if (loc) {
      url.searchParams.set(this.#metadataCache.findLoc, loc);
    }

    return {
      title,
      url: url.toString(),
      subjectExactMatch: false,
      locationParam: this.#metadataCache.findLoc,
      hasLocationSign: false,
      icon_blob: this.#metadataCache.iconBlob,
      source: ml.source,
      provider: ml.provider,
    };
  }

  /**
   * TODO Bug 1926782: ML suggestions don't include an icon, score, or URL, so
   * for now we directly query the Rust backend with a known Yelp keyword and
   * location to get all of that information and then cache it in
   * `#metadataCache`. If the known Yelp suggestion is absent for some reason,
   * we fall back to hardcoded values. This is a tad hacky and we should come up
   * with something better.
   *
   * @returns {object}
   *   The metadata cache.
   */
  async #makeMetadataCache() {
    let cache;

    this.logger.debug("Querying Rust backend to populate metadata cache");
    let rs = await lazy.QuickSuggest.rustBackend.query("coffee in atlanta", [
      "Yelp",
    ]);
    if (!rs.length) {
      this.logger.debug("Rust didn't return any Yelp suggestions!");
      cache = {};
    } else {
      let suggestion = rs[0];
      let url = new URL(suggestion.url);
      let findParamWithValue = value => {
        let tuple = [...url.searchParams.entries()].find(
          ([_, v]) => v == value
        );
        return tuple?.[0];
      };
      cache = {
        iconBlob: suggestion.icon_blob,
        score: suggestion.score,
        urlOrigin: url.origin,
        urlPathname: url.pathname,
        findDesc: findParamWithValue("coffee"),
        findLoc: findParamWithValue("atlanta"),
      };
    }

    let defaults = {
      urlOrigin: "https://www.yelp.com",
      urlPathname: "/search",
      findDesc: "find_desc",
      findLoc: "find_loc",
      score: 0.25,
    };
    for (let [key, value] of Object.entries(defaults)) {
      if (cache[key] === undefined) {
        cache[key] = value;
      }
    }

    return cache;
  }

  _test_invalidateMetadataCache() {
    this.#metadataCache = null;
  }

  #metadataCache = null;
}
