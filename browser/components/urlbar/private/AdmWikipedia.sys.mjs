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

const NONSPONSORED_IAB_CATEGORIES = new Set(["5 - Education"]);

/**
 * A feature that manages sponsored adM and non-sponsored Wikpedia (sometimes
 * called "expanded Wikipedia") suggestions in remote settings.
 */
export class AdmWikipedia extends BaseFeature {
  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored") ||
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored")
    );
  }

  get enablingPreferences() {
    return [
      "suggest.quicksuggest.nonsponsored",
      "suggest.quicksuggest.sponsored",
    ];
  }

  get merinoProvider() {
    return "adm";
  }

  get rustSuggestionTypes() {
    return ["Amp", "Wikipedia"];
  }

  isSuggestionSponsored(suggestion) {
    return suggestion.source == "rust"
      ? suggestion.provider == "Amp"
      : !NONSPONSORED_IAB_CATEGORIES.has(suggestion.iab_category);
  }

  getSuggestionTelemetryType(suggestion) {
    return suggestion.is_sponsored ? "adm_sponsored" : "adm_nonsponsored";
  }

  isRustSuggestionTypeEnabled(type) {
    switch (type) {
      case "Amp":
        return lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored");
      case "Wikipedia":
        return lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored");
    }
    this.logger.error("Unknown Rust suggestion type", { type });
    return false;
  }

  makeResult(queryContext, suggestion) {
    let originalUrl;
    if (suggestion.source == "rust") {
      // The Rust backend defines `rawUrl` on AMP suggestions, and its value is
      // what we on desktop call the `originalUrl`, i.e., it's a URL that may
      // contain timestamp templates. Rust does not define `rawUrl` for
      // Wikipedia suggestions, but we have historically included `originalUrl`
      // for both AMP and Wikipedia even though Wikipedia URLs never contain
      // timestamp templates. So, when setting `originalUrl`, fall back to `url`
      // for suggestions without `rawUrl`.
      originalUrl = suggestion.rawUrl ?? suggestion.url;

      // The Rust backend uses camelCase instead of snake_case, and it excludes
      // some properties in non-sponsored suggestions that we expect, so convert
      // the Rust suggestion to a suggestion object we expect here on desktop.
      let desktopSuggestion = {
        title: suggestion.title,
        url: suggestion.url,
        is_sponsored: suggestion.is_sponsored,
        full_keyword: suggestion.fullKeyword,
      };
      if (suggestion.is_sponsored) {
        desktopSuggestion.impression_url = suggestion.impressionUrl;
        desktopSuggestion.click_url = suggestion.clickUrl;
        desktopSuggestion.block_id = suggestion.blockId;
        desktopSuggestion.advertiser = suggestion.advertiser;
        desktopSuggestion.iab_category = suggestion.iabCategory;
      } else {
        desktopSuggestion.advertiser = "Wikipedia";
        desktopSuggestion.iab_category = "5 - Education";
      }
      suggestion = desktopSuggestion;
    } else {
      // Replace the suggestion's template substrings, but first save the
      // original URL before its timestamp template is replaced.
      originalUrl = suggestion.url;
      lazy.QuickSuggest.replaceSuggestionTemplates(suggestion);
    }

    let payload = {
      originalUrl,
      url: suggestion.url,
      title: suggestion.title,
      isSponsored: suggestion.is_sponsored,
      requestId: suggestion.request_id,
      urlTimestampIndex: suggestion.urlTimestampIndex,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredBlockId: suggestion.block_id,
      sponsoredAdvertiser: suggestion.advertiser,
      sponsoredIabCategory: suggestion.iab_category,
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      isManageable: true,
    };

    let isAmpTopPick =
      suggestion.is_sponsored &&
      lazy.UrlbarPrefs.get("quickSuggestAmpTopPickCharThreshold") &&
      lazy.UrlbarPrefs.get("quickSuggestAmpTopPickCharThreshold") <=
        queryContext.trimmedLowerCaseSearchString.length;

    payload.qsSuggestion = [
      suggestion.full_keyword,
      isAmpTopPick
        ? lazy.UrlbarUtils.HIGHLIGHT.TYPED
        : lazy.UrlbarUtils.HIGHLIGHT.SUGGESTED,
    ];

    let result = new lazy.UrlbarResult(
      lazy.UrlbarUtils.RESULT_TYPE.URL,
      lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );

    if (suggestion.is_sponsored) {
      result.isRichSuggestion = true;
      if (isAmpTopPick) {
        result.isBestMatch = true;
        result.suggestedIndex = 1;
      } else {
        if (lazy.UrlbarPrefs.get("quickSuggestSponsoredPriority")) {
          result.isBestMatch = true;
          result.suggestedIndex = 1;
        } else {
          result.richSuggestionIconSize = 16;
        }
        result.payload.descriptionL10n = {
          id: "urlbar-result-action-sponsored",
        };
      }
    }

    return result;
  }
}
