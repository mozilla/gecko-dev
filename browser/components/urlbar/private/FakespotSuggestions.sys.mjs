/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "icon",
      tag: "img",
      classList: ["urlbarView-favicon"],
    },
    {
      name: "body",
      tag: "span",
      overflowable: true,
      children: [
        {
          name: "title",
          tag: "span",
          classList: ["urlbarView-title"],
        },
        {
          name: "description",
          tag: "span",
          children: [
            {
              name: "rating-five-stars",
              tag: "moz-five-star",
            },
            {
              name: "rating-and-total-reviews",
              tag: "span",
            },
            {
              name: "badge",
              tag: "span",
            },
          ],
        },
        {
          name: "footer",
          tag: "span",
        },
      ],
    },
  ],
};

const REVIEWS_OVERFLOW = 99999;

/**
 * A feature that supports Fakespot suggestions.
 */
export class FakespotSuggestions extends BaseFeature {
  constructor() {
    super();
    lazy.UrlbarResult.addDynamicResultType("fakespot");
    lazy.UrlbarView.addDynamicViewTemplate("fakespot", VIEW_TEMPLATE);
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      lazy.UrlbarPrefs.get("fakespotFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.fakespot")
    );
  }

  get enablingPreferences() {
    return ["suggest.quicksuggest.sponsored", "suggest.fakespot"];
  }

  get rustSuggestionTypes() {
    return ["Fakespot"];
  }

  getSuggestionTelemetryType() {
    return "fakespot";
  }

  makeResult(queryContext, suggestion, _searchString) {
    if (!this.isEnabled) {
      return null;
    }

    const payload = {
      url: suggestion.url,
      title: [suggestion.title, lazy.UrlbarUtils.HIGHLIGHT.TYPED],
      rating: Number(suggestion.rating),
      totalReviews: Number(suggestion.totalReviews),
      fakespotGrade: suggestion.fakespotGrade,
      shouldNavigate: true,
      dynamicType: "fakespot",
    };

    const result = new lazy.UrlbarResult(
      lazy.UrlbarUtils.RESULT_TYPE.DYNAMIC,
      lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
      ...lazy.UrlbarResult.payloadAndSimpleHighlights(
        queryContext.tokens,
        payload
      )
    );

    return result;
  }

  getViewUpdate(result) {
    return {
      icon: {
        attributes: {
          src: lazy.UrlbarUtils.ICON.DEFAULT,
        },
      },
      "rating-five-stars": {
        attributes: {
          rating: result.payload.rating,
        },
      },
      "rating-and-total-reviews": {
        l10n:
          result.payload.totalReviews > REVIEWS_OVERFLOW
            ? {
                id: "firefox-suggest-fakespot-rating-and-total-reviews-overflow",
                args: {
                  rating: result.payload.rating,
                  totalReviews: REVIEWS_OVERFLOW,
                },
              }
            : {
                id: "firefox-suggest-fakespot-rating-and-total-reviews",
                args: {
                  rating: result.payload.rating,
                  totalReviews: result.payload.totalReviews,
                },
              },
      },
      badge: {
        l10n: {
          id: "firefox-suggest-fakespot-badge",
        },
        attributes: {
          grade: result.payload.fakespotGrade,
        },
      },
      footer: {
        l10n: {
          id: "firefox-suggest-fakespot-sponsored",
        },
      },
    };
  }
}
