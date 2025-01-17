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

/**
 * A feature that manages AMP suggestions.
 */
export class AmpSuggestions extends SuggestProvider {
  get shouldEnable() {
    return lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored");
  }

  get enablingPreferences() {
    return ["suggest.quicksuggest.sponsored"];
  }

  get merinoProvider() {
    return "adm";
  }

  get rustSuggestionTypes() {
    return ["Amp"];
  }

  isSuggestionSponsored() {
    return true;
  }

  getSuggestionTelemetryType() {
    return "adm_sponsored";
  }

  isRustSuggestionTypeEnabled() {
    return lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored");
  }

  makeResult(queryContext, suggestion) {
    let originalUrl;
    if (suggestion.source == "rust") {
      // The Rust backend defines `rawUrl` on AMP suggestions, and its value is
      // what we on desktop call the `originalUrl`, i.e., it's a URL that may
      // contain timestamp templates.
      originalUrl = suggestion.rawUrl;
    } else {
      // Replace the suggestion's template substrings, but first save the
      // original URL before its timestamp template is replaced.
      originalUrl = suggestion.url;
      lazy.QuickSuggest.replaceSuggestionTemplates(suggestion);

      // Normalize the Merino suggestion so it has camelCased properties like
      // Rust suggestions.
      suggestion = {
        title: suggestion.title,
        url: suggestion.url,
        is_sponsored: suggestion.is_sponsored,
        fullKeyword: suggestion.full_keyword,
        impressionUrl: suggestion.impression_url,
        clickUrl: suggestion.click_url,
        blockId: suggestion.block_id,
        advertiser: suggestion.advertiser,
        iabCategory: suggestion.iab_category,
        requestId: suggestion.request_id,
      };
    }

    let payload = {
      originalUrl,
      url: suggestion.url,
      title: suggestion.title,
      isSponsored: suggestion.is_sponsored,
      requestId: suggestion.requestId,
      urlTimestampIndex: suggestion.urlTimestampIndex,
      sponsoredImpressionUrl: suggestion.impressionUrl,
      sponsoredClickUrl: suggestion.clickUrl,
      sponsoredBlockId: suggestion.blockId,
      sponsoredAdvertiser: suggestion.advertiser,
      sponsoredIabCategory: suggestion.iabCategory,
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
      isManageable: true,
    };

    let isTopPick =
      lazy.UrlbarPrefs.get("quickSuggestAmpTopPickCharThreshold") &&
      lazy.UrlbarPrefs.get("quickSuggestAmpTopPickCharThreshold") <=
        queryContext.trimmedLowerCaseSearchString.length;

    payload.qsSuggestion = [
      suggestion.fullKeyword,
      isTopPick
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

    result.isRichSuggestion = true;
    if (isTopPick) {
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
