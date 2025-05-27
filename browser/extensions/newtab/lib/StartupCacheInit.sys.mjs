/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "resource://newtab/common/Actions.mjs";

const PREF_SPOCS_STARTUPCACHE_ENABLED =
  "discoverystream.spocs.startupCache.enabled";
const PREF_STARTUPCACHE_FEED = "feeds.startupcacheinit";

/**
 * StartupCacheInit - Startup cache hydrates from a previous state.
 *                    However, weather, sponsored content, and custom wallpapers
 *                    are not cached.
 *                    Weather sponsored content, or custom wallpapers
 *                    can update before we fully hydrate.
 *                    So this feed watches these feeds and updates later after we hydrate.
 *                    We render this feed inert after hydrating from cache or not.
 */
export class StartupCacheInit {
  constructor() {
    // Internal state for checking if we've intialized this feed.
    this.loaded = false;
    this.TopsitesUpdatedReply = false;
    this.DiscoveryStreamSpocsUpdateReply = false;
    this.WeatherUpdateReply = false;
    this.CustomWallpaperUpdateReply = false;
  }

  stateRequestReply(target) {
    // Reply with any updates if we have them.
    if (this.TopsitesUpdatedReply) {
      this.sendTopsitesUpdatedReply(target);
    }
    // Reply with any updates if we have them.
    if (this.DiscoveryStreamSpocsUpdateReply) {
      this.sendDiscoveryStreamSpocsUpdateReply(target);
    }
    // Reply with any updates if we have them.
    if (this.WeatherUpdateReply) {
      this.sendWeatherUpdateReply(target);
    }
    // Reply with any updates if we have them.
    if (this.CustomWallpaperUpdateReply) {
      this.sendCustomWallpaperUpdateReply(target);
    }
  }

  // This grabs the current main reducer state for TopSites rows,
  // and sends it to the startup cache content reducer.
  sendTopsitesUpdatedReply(target) {
    const links = this.store.getState().TopSites.rows;
    const action = {
      type: at.TOP_SITES_UPDATED,
      data: {
        links,
      },
    };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  // This grabs the current main reducer state for DiscoveryStream spocs,
  // and sends it to the startup cache content reducer.
  sendDiscoveryStreamSpocsUpdateReply(target) {
    const spocsState = this.store.getState().DiscoveryStream.spocs;
    const action = {
      type: at.DISCOVERY_STREAM_SPOCS_UPDATE,
      data: {
        lastUpdated: spocsState.lastUpdated,
        spocs: spocsState.data,
      },
    };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  // This grabs the current main reducer state for TopSites rows,
  // and sends it to the startup cache content reducer.
  sendWeatherUpdateReply(target) {
    const { Weather } = this.store.getState();
    const action = {
      type: at.WEATHER_UPDATE,
      data: {
        suggestions: Weather.suggestions,
        lastUpdated: Weather.lastUpdated,
        locationData: Weather.locationData,
      },
    };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  // This grabs the current main reducer state for Wallpapers uploaded wallpaper,
  // and sends it to the startup cache content reducer.
  sendCustomWallpaperUpdateReply(target) {
    const { Wallpapers } = this.store.getState();
    const action = {
      type: at.WALLPAPERS_CUSTOM_SET,
      data: Wallpapers.uploadedWallpaper,
    };
    this.store.dispatch(ac.OnlyToOneContent(action, target));
  }

  uninitFeed() {
    this.store.uninitFeed(PREF_STARTUPCACHE_FEED, { type: at.UNINIT });
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        this.loaded = true;
        break;
      case at.UNINIT:
        this.loaded = false;
        this.TopsitesUpdatedReply = false;
        this.DiscoveryStreamSpocsUpdateReply = false;
        this.WeatherUpdateReply = false;
        this.CustomWallpaperUpdateReply = false;
        break;
      // We either get NEW_TAB_STATE_REQUEST_STARTUPCACHE
      // or NEW_TAB_STATE_REQUEST_WITHOUT_STARTUPCACHE
      // but not both.
      case at.NEW_TAB_STATE_REQUEST_STARTUPCACHE:
        // Reply if we have not yet replied, and are receiving a request.
        if (this.loaded) {
          this.stateRequestReply(action.meta.fromTarget);
          // If we replied, we can turn off this feed.
          this.uninitFeed();
        }
        break;
      case at.NEW_TAB_STATE_REQUEST_WITHOUT_STARTUPCACHE:
        if (this.loaded) {
          // If we rendered without startup cache, we can turn off this feed.
          this.uninitFeed();
        }
        break;
      case at.TOP_SITES_UPDATED:
        // We are receiving a TopSites event and have not yet replied, store this and reply later.
        if (this.loaded) {
          this.TopsitesUpdatedReply = true;
        }
        break;
      case at.DISCOVERY_STREAM_SPOCS_UPDATE:
        if (this.loaded) {
          // Turn this off if startupcache for spocs is on.
          const spocsStartupCacheEnabled =
            this.store.getState().Prefs.values[PREF_SPOCS_STARTUPCACHE_ENABLED];
          if (!spocsStartupCacheEnabled) {
            // We are receiving a DiscoveryStream event and have not yet replied,
            // store this and reply later.
            this.DiscoveryStreamSpocsUpdateReply = true;
          }
        }
        break;
      case at.WEATHER_UPDATE:
        if (this.loaded) {
          // We are receiving a Weather event and have not yet replied, store this and reply later.
          this.WeatherUpdateReply = true;
        }
        break;
      case at.WALLPAPERS_CUSTOM_SET:
        if (this.loaded) {
          // We are receiving a Custom Wallpaper event and have not yet replied, store this and reply later.
          this.CustomWallpaperUpdateReply = true;
        }
        break;
    }
  }
}
