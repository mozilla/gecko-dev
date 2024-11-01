/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

const MERINO_PROVIDER = "accuweather";
const MERINO_TIMEOUT_MS = 5000; // 5s

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

// The mean Earth radius used in distance calculations.
const EARTH_RADIUS_KM = 6371.009;

const RESULT_MENU_COMMAND = {
  INACCURATE_LOCATION: "inaccurate_location",
  MANAGE: "manage",
  NOT_INTERESTED: "not_interested",
  NOT_RELEVANT: "not_relevant",
  SHOW_LESS_FREQUENTLY: "show_less_frequently",
};

const WEATHER_PROVIDER_DISPLAY_NAME = "AccuWeather";

const WEATHER_DYNAMIC_TYPE = "weather";
const WEATHER_VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "currentConditions",
      tag: "span",
      children: [
        {
          name: "currently",
          tag: "div",
        },
        {
          name: "currentTemperature",
          tag: "div",
          children: [
            {
              name: "temperature",
              tag: "span",
            },
            {
              name: "weatherIcon",
              tag: "img",
            },
          ],
        },
      ],
    },
    {
      name: "summary",
      tag: "span",
      overflowable: true,
      children: [
        {
          name: "top",
          tag: "div",
          children: [
            {
              name: "topNoWrap",
              tag: "span",
              children: [
                { name: "title", tag: "span", classList: ["urlbarView-title"] },
                {
                  name: "titleSeparator",
                  tag: "span",
                  classList: ["urlbarView-title-separator"],
                },
              ],
            },
            {
              name: "url",
              tag: "span",
              classList: ["urlbarView-url"],
            },
          ],
        },
        {
          name: "middle",
          tag: "div",
          children: [
            {
              name: "middleNoWrap",
              tag: "span",
              overflowable: true,
              children: [
                {
                  name: "summaryText",
                  tag: "span",
                },
                {
                  name: "summaryTextSeparator",
                  tag: "span",
                },
                {
                  name: "highLow",
                  tag: "span",
                },
              ],
            },
            {
              name: "highLowWrap",
              tag: "span",
            },
          ],
        },
        {
          name: "bottom",
          tag: "div",
        },
      ],
    },
  ],
};

/**
 * A feature that periodically fetches weather suggestions from Merino.
 */
export class Weather extends BaseFeature {
  constructor(...args) {
    super(...args);
    lazy.UrlbarResult.addDynamicResultType(WEATHER_DYNAMIC_TYPE);
    lazy.UrlbarView.addDynamicViewTemplate(
      WEATHER_DYNAMIC_TYPE,
      WEATHER_VIEW_TEMPLATE
    );
  }

  get shouldEnable() {
    return (
      lazy.UrlbarPrefs.get("suggest.quicksuggest.sponsored") &&
      lazy.UrlbarPrefs.get("weatherFeatureGate") &&
      lazy.UrlbarPrefs.get("suggest.weather")
    );
  }

  get enablingPreferences() {
    return ["suggest.quicksuggest.sponsored", "suggest.weather"];
  }

  get rustSuggestionTypes() {
    return ["Weather"];
  }

  isSuggestionSponsored(_suggestion) {
    return true;
  }

  getSuggestionTelemetryType() {
    return "weather";
  }

  /**
   * @returns {number}
   *   The minimum prefix length of a weather keyword the user must type to
   *   trigger the suggestion. Note that the strings returned from `keywords`
   *   already take this into account. The min length is determined from the
   *   first config source below whose value is non-zero. If no source has a
   *   non-zero value, zero will be returned, and `this.keywords` will contain
   *   only full keywords.
   *
   *   1. The `weather.minKeywordLength` pref, which is set when the user
   *      increments the min length
   *   2. `weatherKeywordsMinimumLength` in Nimbus
   *   3. `min_keyword_length` in the weather record in remote settings (i.e.,
   *      the weather config)
   */
  get minKeywordLength() {
    let minLength =
      lazy.UrlbarPrefs.get("weather.minKeywordLength") ||
      lazy.UrlbarPrefs.get("weatherKeywordsMinimumLength") ||
      this.#config.minKeywordLength ||
      0;
    return Math.max(minLength, 0);
  }

  /**
   * @returns {boolean}
   *   Weather the min keyword length can be incremented. A cap on the min
   *   length can be set in remote settings and Nimbus.
   */
  get canIncrementMinKeywordLength() {
    let nimbusMax =
      lazy.UrlbarPrefs.get("weatherKeywordsMinimumLengthCap") || 0;

    let maxKeywordLength;
    if (nimbusMax) {
      // In Nimbus, the cap is the max keyword length.
      maxKeywordLength = nimbusMax;
    } else {
      // In the RS config, the cap is the max number of times the user can click
      // "Show less frequently". The max keyword length is therefore the initial
      // min length plus the cap.
      let min = this.#config.minKeywordLength;
      let cap = lazy.QuickSuggest.backend.config?.showLessFrequentlyCap;
      if (min && cap) {
        maxKeywordLength = min + cap;
      }
    }

    return !maxKeywordLength || this.minKeywordLength < maxKeywordLength;
  }

  enable(enabled) {
    if (!enabled) {
      this.#merino = null;
    }
  }

  /**
   * Increments the minimum prefix length of a weather keyword the user must
   * type to trigger the suggestion, if possible. A cap on the min length can be
   * set in remote settings and Nimbus, and if the cap has been reached, the
   * length is not incremented.
   */
  incrementMinKeywordLength() {
    if (this.canIncrementMinKeywordLength) {
      lazy.UrlbarPrefs.set(
        "weather.minKeywordLength",
        this.minKeywordLength + 1
      );
    }
  }

  async filterSuggestions(suggestions) {
    // If the query didn't include a city, Rust will return at most one
    // suggestion. If the query matched multiple cities, Rust will return one
    // suggestion per city. All suggestions will have the same score, and
    // they'll be ordered by population size from largest to smallest.
    if (suggestions.length <= 1) {
      return suggestions;
    }
    let geo = await lazy.QuickSuggest.geolocation();
    return [
      this.#bestSuggestionByDistance(geo, suggestions) ||
        this.#bestSuggestionByRegion(geo, suggestions) ||
        suggestions[0],
    ];
  }

  /**
   * Returns the suggestion with the city nearest the client's geolocation based
   * on the great-circle distance between the coordinates [1]. This isn't
   * necessarily super accurate, but that's OK since it's stable and accurate
   * enough to find a good matching suggestion.
   *
   * [1] https://en.wikipedia.org/wiki/Great-circle_distance
   *
   * @param {object} geo
   *   The `geolocation` object returned by Merino's geolocation provider. It's
   *   expected to look like the following, but we gracefully handle exceptions:
   *
   *     `{ location: { latitude, longitude, radius }}`
   *
   *   The coordinates are expected to be in decimal and the radius is expected
   *   to be in km.
   * @param {Array} suggestions
   *   Array of candidate weather suggestions.
   * @returns {object|null}
   *   The nearest suggestion as described above. If there are multiple nearest
   *   cities within the accuracy radius, the most populous one is returned. If
   *   the `geo` does not include a location or coordinates, null is returned.
   */
  #bestSuggestionByDistance(geo, suggestions) {
    let geoLat = geo?.location?.latitude;
    let geoLong = geo?.location?.longitude;
    if (isNaN(geoLat) || isNaN(geoLong)) {
      return null;
    }

    // All distances are in km.
    [geoLat, geoLong] = [geoLat, geoLong].map(toRadians);
    let geoLatSin = Math.sin(geoLat);
    let geoLatCos = Math.cos(geoLat);
    let geoRadius = geo?.location?.radius || 5;

    let best;
    let dMin = Infinity;
    for (let s of suggestions) {
      let [sLat, sLong] = [s.latitude, s.longitude].map(toRadians);
      let d =
        EARTH_RADIUS_KM *
        Math.acos(
          geoLatSin * Math.sin(sLat) +
            geoLatCos * Math.cos(sLat) * Math.cos(Math.abs(geoLong - sLong))
        );
      if (
        !best ||
        // `s` is closer to the client than `best`.
        d + geoRadius < dMin ||
        // `s` is the same distance from the client as `best`, i.e., the
        // difference between `s` and `best` is within the accuracy radius.
        // Choose `s` if it has a larger population.
        (Math.abs(d - dMin) <= geoRadius && best.population < s.population)
      ) {
        dMin = d;
        best = s;
      }
    }

    return best;
  }

  /**
   * Returns the first suggestion with a city located in the same region and
   * country as the client's geolocation. If there is no such suggestion, the
   * first suggestion in the same country is returned. If there is no suggestion
   * in the same country, null is returned. Since `suggestions` is ordered by
   * population, if multiple cities match any of these criteria, the one that's
   * returned will be the most populous.
   *
   * @param {object} geo
   *   The `geolocation` object returned by Merino's geolocation provider. It's
   *   expected to look like the following, but we gracefully handle exceptions:
   *
   *     `{ region_code, country_code }`
   * @param {Array} suggestions
   *   Array of candidate weather suggestions.
   * @returns {object|null}
   *   The suggestion as described above or null.
   */
  #bestSuggestionByRegion(geo, suggestions) {
    let region = geo?.region_code?.toLowerCase();
    let country = geo?.country_code?.toLowerCase();
    if (!region && !country) {
      return null;
    }

    let sameCountrySuggestion = null;
    for (let s of suggestions) {
      let sameRegion = s.region.toLowerCase() == region;
      let sameCountry = s.country.toLowerCase() == country;
      if (sameRegion && sameCountry) {
        // This is the most populous city (since suggestions are ordered by
        // population) in the client's region. Can't get better than this.
        return s;
      }
      if (sameCountry && !sameCountrySuggestion) {
        sameCountrySuggestion = s;
      }
    }

    return sameCountrySuggestion;
  }

  async makeResult(queryContext, suggestion, searchString) {
    // The Rust component doesn't enforce a minimum keyword length, so discard
    // the suggestion if the search string isn't long enough. This conditional
    // will always be false for the JS backend since in that case keywords are
    // never shorter than `minKeywordLength`.
    if (searchString.length < this.minKeywordLength) {
      return null;
    }

    if (!this.#merino) {
      this.#merino = new lazy.MerinoClient(this.constructor.name);
    }

    // Set up location params to pass to Merino. We need to null-check each
    // suggestion property because `MerinoClient` will stringify null values.
    let otherParams = {};
    for (let key of ["city", "region", "country"]) {
      if (suggestion[key]) {
        otherParams[key] = suggestion[key];
      }
    }

    let merino = this.#merino;
    let fetchInstance = (this.#fetchInstance = {});
    let suggestions = await merino.fetch({
      query: "",
      otherParams,
      providers: [MERINO_PROVIDER],
      timeoutMs: this.#timeoutMs,
      extraLatencyHistogram: HISTOGRAM_LATENCY,
      extraResponseHistogram: HISTOGRAM_RESPONSE,
    });
    if (fetchInstance != this.#fetchInstance || merino != this.#merino) {
      return null;
    }

    if (!suggestions.length) {
      return null;
    }
    suggestion = suggestions[0];

    let unit = Services.locale.regionalPrefsLocales[0] == "en-US" ? "f" : "c";
    return Object.assign(
      new lazy.UrlbarResult(
        lazy.UrlbarUtils.RESULT_TYPE.DYNAMIC,
        lazy.UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          url: suggestion.url,
          iconId: suggestion.current_conditions.icon_id,
          requestId: suggestion.request_id,
          dynamicType: WEATHER_DYNAMIC_TYPE,
          city: suggestion.city_name,
          temperatureUnit: unit,
          temperature: suggestion.current_conditions.temperature[unit],
          currentConditions: suggestion.current_conditions.summary,
          forecast: suggestion.forecast.summary,
          high: suggestion.forecast.high[unit],
          low: suggestion.forecast.low[unit],
        }
      ),
      {
        showFeedbackMenu: true,
        suggestedIndex: searchString ? 1 : 0,
      }
    );
  }

  getViewUpdate(result) {
    let uppercaseUnit = result.payload.temperatureUnit.toUpperCase();
    return {
      currently: {
        l10n: {
          id: "firefox-suggest-weather-currently",
          cacheable: true,
        },
      },
      temperature: {
        l10n: {
          id: "firefox-suggest-weather-temperature",
          args: {
            value: result.payload.temperature,
            unit: uppercaseUnit,
          },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      weatherIcon: {
        attributes: { iconId: result.payload.iconId },
      },
      title: {
        l10n: {
          id: "firefox-suggest-weather-title",
          args: { city: result.payload.city },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      url: {
        textContent: result.payload.url,
      },
      summaryText: lazy.UrlbarPrefs.get("weatherSimpleUI")
        ? { textContent: result.payload.currentConditions }
        : {
            l10n: {
              id: "firefox-suggest-weather-summary-text",
              args: {
                currentConditions: result.payload.currentConditions,
                forecast: result.payload.forecast,
              },
              cacheable: true,
              excludeArgsFromCacheKey: true,
            },
          },
      highLow: {
        l10n: {
          id: "firefox-suggest-weather-high-low",
          args: {
            high: result.payload.high,
            low: result.payload.low,
            unit: uppercaseUnit,
          },
          cacheable: true,
          excludeArgsFromCacheKey: true,
        },
      },
      highLowWrap: {
        l10n: {
          id: "firefox-suggest-weather-high-low",
          args: {
            high: result.payload.high,
            low: result.payload.low,
            unit: uppercaseUnit,
          },
        },
      },
      bottom: {
        l10n: {
          id: "firefox-suggest-weather-sponsored",
          args: { provider: WEATHER_PROVIDER_DISPLAY_NAME },
          cacheable: true,
        },
      },
    };
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

    if (this.canIncrementMinKeywordLength) {
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

  handleCommand(view, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMAND.MANAGE:
        // "manage" is handled by UrlbarInput, no need to do anything here.
        break;
      // selType == "dismiss" when the user presses the dismiss key shortcut.
      case "dismiss":
      case RESULT_MENU_COMMAND.NOT_INTERESTED:
      case RESULT_MENU_COMMAND.NOT_RELEVANT:
        this.logger.info("Dismissing weather result");
        lazy.UrlbarPrefs.set("suggest.weather", false);
        result.acknowledgeDismissalL10n = {
          id: "firefox-suggest-dismissal-acknowledgment-all",
        };
        view.controller.removeResult(result);
        break;
      case RESULT_MENU_COMMAND.INACCURATE_LOCATION:
        // Currently the only way we record this feedback is in the Glean
        // engagement event. As with all commands, it will be recorded with an
        // `engagement_type` value that is the command's name, in this case
        // `inaccurate_location`.
        view.acknowledgeFeedback(result);
        break;
      case RESULT_MENU_COMMAND.SHOW_LESS_FREQUENTLY:
        view.acknowledgeFeedback(result);
        this.incrementMinKeywordLength();
        if (!this.canIncrementMinKeywordLength) {
          view.invalidateResultMenuCommands();
        }
        break;
    }
  }

  get #config() {
    let { rustBackend } = lazy.QuickSuggest;
    let config = rustBackend.isEnabled
      ? rustBackend.getConfigForSuggestionType(this.rustSuggestionTypes[0])
      : null;
    return config || {};
  }

  get _test_merino() {
    return this.#merino;
  }

  _test_setTimeoutMs(ms) {
    this.#timeoutMs = ms < 0 ? MERINO_TIMEOUT_MS : ms;
  }

  #fetchInstance = null;
  #merino = null;
  #timeoutMs = MERINO_TIMEOUT_MS;
}

function toRadians(deg) {
  return (deg * Math.PI) / 180;
}
