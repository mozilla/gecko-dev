/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SuggestProvider } from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
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

const NONSPONSORED_IAB_CATEGORIES = new Set(["5 - Education"]);

/**
 * A feature that manages sponsored adM and non-sponsored Wikpedia (sometimes
 * called "expanded Wikipedia") suggestions in remote settings.
 */
export class AdmWikipedia extends SuggestProvider {
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

  onImpression(state, queryContext, controller, featureResults, details) {
    // For the purpose of the `quick-suggest` impression ping, "impression"
    // means that one of these suggestions was visible at the time of an
    // engagement regardless of the engagement type or engagement result, so
    // submit the ping if `state` is "engagement".
    if (state == "engagement") {
      for (let result of featureResults) {
        this.#submitQuickSuggestImpressionPing({
          result,
          queryContext,
          details,
        });
      }
    }
  }

  onEngagement(queryContext, controller, details, _searchString) {
    let { result } = details;

    // Handle commands. These suggestions support the Dismissal and Manage
    // commands. Dismissal is the only one we need to handle here. `UrlbarInput`
    // handles Manage.
    if (details.selType == "dismiss") {
      lazy.QuickSuggest.blockedSuggestions.add(result.payload.originalUrl);
      controller.removeResult(result);
    }

    // A `quick-suggest` impression ping must always be submitted on engagement
    // regardless of engagement type. Normally we do that in `onImpression()`,
    // but that's not called when the session remains ongoing, so in that case,
    // submit the impression ping now.
    if (details.isSessionOngoing) {
      this.#submitQuickSuggestImpressionPing({ queryContext, result, details });
    }

    // Submit the `quick-suggest` engagement ping.
    let pingData;
    switch (details.selType) {
      case "quicksuggest":
        pingData = {
          pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION,
          reportingUrl: result.payload.sponsoredClickUrl,
        };
        break;
      case "dismiss":
        pingData = {
          pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
          iabCategory: result.payload.sponsoredIabCategory,
        };
        break;
    }
    if (pingData) {
      this.#submitQuickSuggestPing({ queryContext, result, ...pingData });
    }
  }

  #submitQuickSuggestPing({ queryContext, result, pingType, ...pingData }) {
    if (queryContext.isPrivate) {
      return;
    }

    let allPingData = {
      pingType,
      ...pingData,
      matchType: result.isBestMatch ? "best-match" : "firefox-suggest",
      // Always use lowercase to make the reporting consistent.
      advertiser: result.payload.sponsoredAdvertiser.toLocaleLowerCase(),
      blockId: result.payload.sponsoredBlockId,
      improveSuggestExperience: lazy.UrlbarPrefs.get(
        "quicksuggest.dataCollection.enabled"
      ),
      // `position` is 1-based, unlike `rowIndex`, which is zero-based.
      position: result.rowIndex + 1,
      suggestedIndex: result.suggestedIndex.toString(),
      suggestedIndexRelativeToGroup: !!result.isSuggestedIndexRelativeToGroup,
      requestId: result.payload.requestId,
      source: result.payload.source,
      contextId: lazy.contextId,
    };

    for (let [gleanKey, value] of Object.entries(allPingData)) {
      let glean = Glean.quickSuggest[gleanKey];
      if (value !== undefined && value !== "") {
        glean.set(value);
      }
    }
    GleanPings.quickSuggest.submit();
  }

  #submitQuickSuggestImpressionPing({ queryContext, result, details }) {
    this.#submitQuickSuggestPing({
      result,
      queryContext,
      pingType: lazy.CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
      isClicked:
        // `selType` == "quicksuggest" if the result itself was clicked. It will
        // be a command name if a command was clicked, e.g., "dismiss".
        result == details.result && details.selType == "quicksuggest",
      reportingUrl: result.payload.sponsoredImpressionUrl,
    });
  }
}
