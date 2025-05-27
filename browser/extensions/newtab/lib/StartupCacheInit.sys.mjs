/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import {
  actionCreators as ac,
  actionTypes as at,
} from "resource://newtab/common/Actions.mjs";

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
    this.CustomWallpaperUpdateReply = false;
  }

  stateRequestReply(target) {
    // Reply with any updates if we have them.
    if (this.CustomWallpaperUpdateReply) {
      this.sendCustomWallpaperUpdateReply(target);
    }
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
      case at.WALLPAPERS_CUSTOM_SET:
        if (this.loaded) {
          // We are receiving a Custom Wallpaper event and have not yet replied, store this and reply later.
          this.CustomWallpaperUpdateReply = true;
        }
        break;
    }
  }
}
