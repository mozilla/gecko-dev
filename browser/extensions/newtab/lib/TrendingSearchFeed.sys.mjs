/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserSearchTelemetry:
    "moz-src:///browser/components/search/BrowserSearchTelemetry.sys.mjs",
  PersistentCache: "resource://newtab/lib/PersistentCache.sys.mjs",
  SearchSuggestionController:
    "moz-src:///toolkit/components/search/SearchSuggestionController.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://newtab/common/Actions.mjs";

const PREF_SHOW_TRENDING_SEARCH = "trendingSearch.enabled";
const PREF_SHOW_TRENDING_SEARCH_SYSTEM = "system.trendingSearch.enabled";
const PREF_TRENDING_SEARCH_DEFAULT = "trendingSearch.defaultSearchEngine";
const TRENDING_SEARCH_UPDATE_TIME = 15 * 60 * 1000; // 15 minutes
const CACHE_KEY = "trending_search";

/**
 * A feature that periodically fetches trending search suggestions for HNT.
 */
export class TrendingSearchFeed {
  constructor() {
    this.initialized = false;
    this.fetchTimer = null;
    this.suggestions = [];
    this.lastUpdated = null;
    this.defaultEngine = null;
    this.cache = this.PersistentCache(CACHE_KEY, true);
  }

  get enabled() {
    const prefs = this.store.getState()?.Prefs.values;
    const trendingSearchEnabled =
      prefs[PREF_SHOW_TRENDING_SEARCH] &&
      prefs[PREF_SHOW_TRENDING_SEARCH_SYSTEM];
    const isGoogle =
      prefs[PREF_TRENDING_SEARCH_DEFAULT]?.toLowerCase() === "google";
    return trendingSearchEnabled && isGoogle;
  }

  async init() {
    this.initialized = true;
    const engine = await Services.search.getDefault();
    this.defaultEngine = engine;
    await this.loadTrendingSearch(true);
  }

  async loadTrendingSearch(isStartup = false) {
    const cachedData = (await this.cache.get()) || {};
    const { trendingSearch } = cachedData;

    // If we have nothing in cache, or cache has expired, we can make a fresh fetch.
    if (
      !trendingSearch?.lastUpdated ||
      !(
        this.Date().now() - trendingSearch?.lastUpdated <
        TRENDING_SEARCH_UPDATE_TIME
      )
    ) {
      await this.fetch(isStartup);
    } else if (!this.lastUpdated) {
      this.suggestions = trendingSearch.suggestions;
      this.lastUpdated = trendingSearch.lastUpdated;
      this.update();
    }
  }

  async fetch() {
    const suggestions = await this.fetchHelper();
    this.suggestions = suggestions;

    if (this.suggestions?.length) {
      this.lastUpdated = this.Date().now();
      await this.cache.set("trendingSearch", {
        suggestions: this.suggestions,
        lastUpdated: this.lastUpdated,
      });
    }
    this.update();
  }

  update() {
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.TRENDING_SEARCH_UPDATE,
        data: this.suggestions,
      })
    );
  }

  async fetchHelper() {
    if (!this.defaultEngine) {
      return null;
    }
    this.suggestionsController = new lazy.SearchSuggestionController();
    this.suggestionsController.maxLocalResults = 0;

    let suggestionPromise = this.suggestionsController.fetch(
      "", // searchTerm
      false, // privateMode
      this.defaultEngine, // engine
      0,
      false, //restrictToEngine
      false, // dedupeRemoteAndLocal
      true // fetchTrending
    );

    let fetchData = await suggestionPromise;

    if (!fetchData) {
      return null;
    }

    let results = [];

    for (let entry of fetchData.remote) {
      // Construct the fully formatted search URL for the current trending result
      const [searchUrl] = await lazy.UrlbarUtils.getSearchQueryUrl(
        this.defaultEngine,
        entry.value
      );

      results.push({
        engine: this.defaultEngine.name,
        suggestion: entry.value,
        lowerCaseSuggestion: entry.value.toLocaleLowerCase(),
        icon: !entry.value ? await this.defaultEngine.getIconUrl() : entry.icon,
        description: entry.description || undefined,
        isRichSuggestion: !!entry.icon,
        searchUrl,
      });
    }
    return results;
  }

  handleSearchTelemetry(browser) {
    lazy.BrowserSearchTelemetry.recordSearch(
      browser,
      this.defaultEngine,
      "newtab"
    );
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        if (this.enabled) {
          await this.init();
        }
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        if (this.enabled) {
          await this.loadTrendingSearch();
        }
        break;
      case at.TRENDING_SEARCH_SUGGESTION_OPEN:
        this.handleSearchTelemetry(action._target.browser);
        break;
      case at.PREF_CHANGED:
        {
          const { name, value } = action.data;

          const isTrendingShowPref =
            name === PREF_SHOW_TRENDING_SEARCH && value;
          const isTrendingDefaultPref = name === PREF_TRENDING_SEARCH_DEFAULT;

          if (isTrendingShowPref || isTrendingDefaultPref) {
            if (this.enabled) {
              await this.loadTrendingSearch();
            }
          }
        }
        break;
    }
  }
}

TrendingSearchFeed.prototype.Date = () => {
  return Date;
};
TrendingSearchFeed.prototype.PersistentCache = (...args) => {
  return new lazy.PersistentCache(...args);
};
