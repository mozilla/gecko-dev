/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {
  Utils: "resource://services-settings/Utils.sys.mjs",
};

ChromeUtils.defineESModuleGetters(lazy, {
  PersistentCache: "resource://activity-stream/lib/PersistentCache.sys.mjs",
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

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://activity-stream/common/Actions.mjs";

// Prefs for AdsFeeds to run
const PREF_UNIFIED_ADS_ADSFEED_ENABLED = "unifiedAds.adsFeed.enabled";
const PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED =
  "unifiedAds.adsFeed.tiles.enabled";
const PREF_UNIFIED_ADS_ADSFEED_SPOCS_ENABLED =
  "unifiedAds.adsFeed.spocs.enabled";

const PREF_UNIFIED_ADS_BLOCKED_LIST = "unifiedAds.blockedAds";
const PREF_UNIFIED_ADS_COUNTS = "discoverystream.placements.tiles.counts";
const PREF_UNIFIED_ADS_ENDPOINT = "unifiedAds.endpoint";
const PREF_UNIFIED_ADS_PLACEMENTS = "discoverystream.placements.tiles";

// Prefs to enable UAPI ad types
const PREF_UNIFIED_ADS_SPOCS_ENABLED = "unifiedAds.spocs.enabled";
const PREF_UNIFIED_ADS_TILES_ENABLED = "unifiedAds.tiles.enabled";

// Primary pref that is toggled when enabling top site sponsored tiles
const PREF_FEED_TOPSITES = "feeds.topsites";
const PREF_SHOW_SPONSORED_TOPSITES = "showSponsoredTopSites";

// Primary pref that is toggled when enabling sponsored stories
const PREF_FEED_SECTIONS_TOPSTORIES = "feeds.section.topstories";
const PREF_SHOW_SPONSORED = "showSponsored";
const PREF_SYSTEM_SHOW_SPONSORED = "system.showSponsored";

const CACHE_KEY = "ads_feed";
const ADS_UPDATE_TIME = 30 * 60 * 1000; // 30 minutes

export class AdsFeed {
  constructor() {
    this.enabled = false;
    this.loaded = false;
    this.lastUpdated = null;
    this.tiles = [];
    this.cache = this.PersistentCache(CACHE_KEY, true);
  }

  async _resetCache() {
    if (this.cache) {
      await this.cache.set("ads", {});
    }
  }

  async resetAdsFeed() {
    await this._resetCache();
    this.tiles = [];
    this.lastUpdated = null;
    this.loaded = false;
    this.enabled = false;
  }

  isEnabled() {
    this.loaded = true;

    // Check if AdsFeed is enabled
    const adsFeedEnabled =
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_ADSFEED_ENABLED];

    // Check all known prefs that top site tiles are enabled
    const tilesEnabled = this.store.getState().Prefs.values[PREF_FEED_TOPSITES];

    const sponsoredTilesEnabled =
      this.store.getState().Prefs.values[PREF_SHOW_SPONSORED_TOPSITES] &&
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_TILES_ENABLED];

    // Check all known prefs that spocs are enabled
    const sponsoredStoriesEnabled =
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_SPOCS_ENABLED] &&
      this.store.getState().Prefs.values[PREF_SYSTEM_SHOW_SPONSORED] &&
      this.store.getState().Prefs.values[PREF_SHOW_SPONSORED];

    const storiesEnabled =
      this.store.getState().Prefs.values[PREF_FEED_SECTIONS_TOPSTORIES];

    // Confirm at least one ads section (tiles, spocs) are enabled to enable the adsfeed
    if (
      (tilesEnabled && sponsoredTilesEnabled) ||
      (storiesEnabled && sponsoredStoriesEnabled)
    ) {
      if (adsFeedEnabled) {
        this.enabled = true;
      }

      return adsFeedEnabled;
    }

    return false;
  }

  /**
   * This thin wrapper around global.fetch makes it easier for us to write
   * automated tests that simulate responses from this fetch.
   */
  fetch(...args) {
    return fetch(...args);
  }

  /**
   * Normalize new Unified Ads API response into
   * previous Contile ads response
   */
  _normalizeTileData(data) {
    const formattedTileData = [];
    const responseTilesData = Object.values(data);

    // Bug 1930653: Confirm response has data before iterating
    if (responseTilesData?.length) {
      for (const tileData of responseTilesData) {
        const [tile] = tileData;

        const formattedData = {
          id: tile.block_key,
          block_key: tile.block_key,
          name: tile.name,
          url: tile.url,
          click_url: tile.callbacks.click,
          image_url: tile.image_url,
          impression_url: tile.callbacks.impression,
          image_size: 200,
        };

        formattedTileData.push(formattedData);
      }
    }

    return { tiles: formattedTileData };
  }

  /**
   * Return object of supported ad types to query from MARS API from the adsfeed file
   * @returns {object}
   */
  getSupportedAdTypes() {
    const supportsAdsFeedTiles =
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_TILES_ENABLED] &&
      this.store.getState().Prefs.values[
        PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED
      ];

    const supportsAdsFeedSpocs =
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_SPOCS_ENABLED] &&
      this.store.getState().Prefs.values[
        PREF_UNIFIED_ADS_ADSFEED_SPOCS_ENABLED
      ];

    return {
      tiles: supportsAdsFeedTiles,
      spocs: supportsAdsFeedSpocs,
    };
  }

  /**
   * Get ads data either from cache or from API and
   * broadcast the data via at.ADS_UPDATE_DATA event
   * @param {boolean} isStartup=false - This is only used for reporting
   * and is passed to the update functions meta attribute
   * @returns {void}
   */
  async getAdsData(isStartup = false) {
    const supportedAdTypes = this.getSupportedAdTypes();
    const cachedData = (await this.cache.get()) || {};
    const { ads } = cachedData;
    const adsCacheValid = ads
      ? this.Date().now() - ads.lastUpdated < ADS_UPDATE_TIME
      : false;

    let data = null;

    // Get new data if necessary or default to cache
    if (!ads?.lastUpdated || !adsCacheValid) {
      // Fetch new data
      data = await this.fetchData(supportedAdTypes);
      data.lastUpdated = this.Date().now();
    } else {
      // Use cached data
      data = ads;
    }

    if (!data) {
      throw new Error(`No data available`);
    }

    // Update tile information if tile data is supported
    if (supportedAdTypes.tiles) {
      this.tiles = data.tiles;
    }

    this.lastUpdated = data.lastUpdated;
    await this.update(isStartup);
  }

  /**
   * Fetch data from the Mozilla Ad Routing Service (MARS) unified ads API
   * This function is designed to get whichever ads types are needed (tiles, spocs)
   * @param {array} supportedAdTypes
   * @returns {object} Response object containing ad information from MARS
   */
  async fetchData(supportedAdTypes) {
    const state = this.store.getState();
    const headers = new Headers();
    headers.append("content-type", "application/json");

    const endpointBaseUrl = state.Prefs.values[PREF_UNIFIED_ADS_ENDPOINT];

    let blockedSponsors =
      this.store.getState().Prefs.values[PREF_UNIFIED_ADS_BLOCKED_LIST];

    // Overwrite URL to Unified Ads endpoint
    const fetchUrl = `${endpointBaseUrl}v1/ads`;

    let placements;
    let counts;

    // Determine which data needs to be fetched
    if (supportedAdTypes.tiles) {
      placements = state.Prefs.values[PREF_UNIFIED_ADS_PLACEMENTS]?.split(`,`)
        .map(s => s.trim())
        .filter(item => item);
      counts = state.Prefs.values[PREF_UNIFIED_ADS_COUNTS]?.split(`,`)
        .map(s => s.trim())
        .filter(item => item)
        .map(item => parseInt(item, 10));
    }

    const response = await this.fetch(fetchUrl, {
      method: "POST",
      headers,
      body: JSON.stringify({
        context_id: lazy.contextId,
        placements: placements.map((placement, index) => ({
          placement,
          count: counts[index],
        })),
        blocks: blockedSponsors.split(","),
      }),
    });

    // If supportedAdTypes tiles type, normalize it!
    if (supportedAdTypes.tiles) {
      const responseData = await response.json();
      const normalizedTileData = this._normalizeTileData(responseData);
      return normalizedTileData;
    }

    return await response.json();
  }

  /**
   * Init function that runs only from onAction at.INIT call.
   * @param {boolean} isStartup=false
   * @returns {void}
   */
  async init(isStartup = false) {
    if (this.isEnabled()) {
      await this.getAdsData(isStartup);
    }
  }

  /**
   * Sets cached data and dispatches ADS_UPDATE_DATA event to update store with new ads data
   * @param {boolean} isStartup
   * @returns {void}
   */
  async update(isStartup) {
    await this.cache.set("ads", {
      tiles: this.tiles,
      lastUpdated: this.lastUpdated,
    });

    // TODO: Submit data based on supportedTypes
    this.store.dispatch(
      ac.BroadcastToContent({
        type: at.ADS_UPDATE_DATA,
        data: {
          tiles: this.tiles,
        },
        meta: {
          isStartup,
        },
      })
    );
  }

  async onPrefChangedAction(action) {
    switch (action.data.name) {
      case PREF_FEED_SECTIONS_TOPSTORIES:
      case PREF_FEED_TOPSITES:
      case PREF_SHOW_SPONSORED_TOPSITES:
      case PREF_SHOW_SPONSORED:
      case PREF_SYSTEM_SHOW_SPONSORED:
      case PREF_UNIFIED_ADS_ADSFEED_TILES_ENABLED:
      case PREF_UNIFIED_ADS_ADSFEED_SPOCS_ENABLED:
      case PREF_UNIFIED_ADS_ADSFEED_ENABLED:
        if (this.isEnabled()) {
          await this.getAdsData(false);
        } else {
          await this.resetAdsFeed();
        }
        break;
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        await this.init(true /* isStartup */);
        break;
      case at.UNINIT:
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        if (this.isEnabled()) {
          await this.getAdsData(false);
        }
        break;
      case at.PREF_CHANGED:
        await this.onPrefChangedAction(action);
        break;
    }
  }
}

/**
 * Creating a thin wrapper around PersistentCache and Date.
 * This makes it easier for us to write automated tests that simulate responses.
 */

AdsFeed.prototype.PersistentCache = (...args) => {
  return new lazy.PersistentCache(...args);
};

AdsFeed.prototype.Date = () => {
  return Date;
};
