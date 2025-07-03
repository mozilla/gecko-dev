/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

import { actionTypes as at } from "resource://newtab/common/Actions.mjs";

const PREF_SYSTEM_SHORTCUTS_PERSONALIZATION =
  "discoverystream.shortcuts.personalization.enabled";

function timeMSToSeconds(timeMS) {
  return Math.round(timeMS / 1000);
}

/**
 * A feature that periodically generates an interest vector for personalized shortcuts.
 */
export class SmartShortcutsFeed {
  constructor() {
    this.loaded = false;
  }

  async reset() {
    this.loaded = false;
  }

  isEnabled() {
    const { values } = this.store.getState().Prefs;
    const systemPref = values[PREF_SYSTEM_SHORTCUTS_PERSONALIZATION];
    const experimentVariable = values.smartShortcutsConfig?.enabled;

    return systemPref || experimentVariable;
  }

  async init() {
    if (this.isEnabled()) {
      this.loaded = true;
    }
  }

  async recordShortcutsInteraction(event_type, data) {
    // We don't need to worry about interactions that don't have a guid.
    if (!data.guid) {
      return;
    }
    const insertValues = {
      guid: data.guid,
      event_type,
      timestamp_s: timeMSToSeconds(this.Date().now()),
      pinned: data.isPinned ? 1 : 0,
      tile_position: data.position,
    };

    let sql = `
      INSERT INTO moz_newtab_shortcuts_interaction (
        place_id, event_type, timestamp_s, pinned, tile_position
      )
      SELECT
        id, :event_type, :timestamp_s, :pinned, :tile_position
      FROM moz_places
      WHERE guid = :guid
    `;

    await lazy.PlacesUtils.withConnectionWrapper(
      "newtab/lib/SmartShortcutsFeed.sys.mjs: recordShortcutsInteraction",
      async db => {
        await db.execute(sql, insertValues);
      }
    );
  }

  async handleTopSitesOrganicImpressionStats(action) {
    switch (action.data?.type) {
      case "impression": {
        await this.recordShortcutsInteraction(0, action.data);
        break;
      }
      case "click": {
        await this.recordShortcutsInteraction(1, action.data);
        break;
      }
    }
  }

  async onPrefChangedAction(action) {
    switch (action.data.name) {
      case PREF_SYSTEM_SHORTCUTS_PERSONALIZATION: {
        await this.init();
        break;
      }
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        await this.init();
        break;
      case at.UNINIT:
        await this.reset();
        break;
      case at.TOP_SITES_ORGANIC_IMPRESSION_STATS:
        if (this.isEnabled()) {
          await this.handleTopSitesOrganicImpressionStats(action);
        }
        break;
      case at.PREF_CHANGED:
        this.onPrefChangedAction(action);
        if (action.data.name === "smartShortcutsConfig") {
          await this.init();
        }
        break;
    }
  }
}

/**
 * Creating a thin wrapper around Date.
 * This makes it easier for us to write automated tests that simulate responses.
 */
SmartShortcutsFeed.prototype.Date = () => {
  return Date;
};
