/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  PersistentCache: "resource://newtab/lib/PersistentCache.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://newtab/common/Actions.mjs";

const CACHE_KEY = "inferred_personalization_feed";
const INTEREST_VECTOR_UPDATE_TIME = 4 * 60 * 60 * 1000; // 4 hours
const PREF_USER_INFERRED_PERSONALIZATION =
  "discoverystream.sections.personalization.inferred.user.enabled";
const PREF_SYSTEM_INFERRED_PERSONALIZATION =
  "discoverystream.sections.personalization.inferred.enabled";

const FORMAT_ENUM = {
  SMALL: 0,
  MEDIUM: 1,
  LARGE: 2,
};

const FORMAT = {
  "small-card": FORMAT_ENUM.SMALL,
  "medium-card": FORMAT_ENUM.MEDIUM,
  "large-card": FORMAT_ENUM.LARGE,
};

/**
 * A feature that periodically generates a interest vector for inferred personalization.
 */
export class InferredPersonalizationFeed {
  constructor() {
    this.loaded = false;
    this.cache = this.PersistentCache(CACHE_KEY, true);
  }

  async reset() {
    if (this.cache) {
      await this.cache.set("interest_vector", {});
    }
    this.loaded = false;
    this.store.dispatch(
      ac.OnlyToMain({
        type: at.INFERRED_PERSONALIZATION_RESET,
      })
    );
  }

  isEnabled() {
    return (
      this.store.getState().Prefs.values[PREF_USER_INFERRED_PERSONALIZATION] &&
      this.store.getState().Prefs.values[PREF_SYSTEM_INFERRED_PERSONALIZATION]
    );
  }

  async init() {
    await this.loadInterestVector(true /* isStartup */);
  }

  async generateInterestVector() {
    // TODO items and model should be props passed in, or fetched in this function.
    // TODO Run items and model to generate interest vector.
  }

  async loadInterestVector(isStartup = false) {
    const cachedData = (await this.cache.get()) || {};
    let { interest_vector } = cachedData;

    // If we have nothing in cache, or cache has expired, we can make a fresh fetch.
    if (
      !interest_vector?.lastUpdated ||
      !(
        this.Date().now() - interest_vector.lastUpdated <
        INTEREST_VECTOR_UPDATE_TIME
      )
    ) {
      interest_vector = {
        data: await this.generateInterestVector(),
        lastUpdated: this.Date().now(),
      };
    }
    await this.cache.set("interest_vector", interest_vector);
    this.loaded = true;
    this.store.dispatch(
      ac.OnlyToMain({
        type: at.INFERRED_PERSONALIZATION_UPDATE,
        data: {
          lastUpdated: interest_vector.lastUpdated,
          interestVector: interest_vector.data,
        },
        meta: {
          isStartup,
        },
      })
    );
  }

  async handleDiscoveryStreamImpressionStats(action) {
    const { tiles } = action.data;

    for (const tile of tiles) {
      const { type, format, pos, topic, section_position, features } = tile;
      if (["organic"].includes(type)) {
        await this.recordInferredPersonalizationImpression({
          format,
          pos,
          topic,
          section_position,
          features,
        });
      }
    }
  }

  async handleDiscoveryStreamUserEvent(action) {
    switch (action.data?.event) {
      case "OPEN_NEW_WINDOW":
      case "CLICK": {
        const { card_type, format, topic, section_position, features } =
          action.data.value ?? {};
        const pos = action.data.action_position;
        if (["organic"].includes(card_type)) {
          await this.recordInferredPersonalizationClick({
            format,
            pos,
            topic,
            section_position,
            features,
          });
        }
        break;
      }
    }
  }

  async recordInferredPersonalizationImpression(tile) {
    await this.recordInferredPersonalizationInteraction(
      "moz_newtab_story_impression",
      tile
    );
  }
  async recordInferredPersonalizationClick(tile) {
    await this.recordInferredPersonalizationInteraction(
      "moz_newtab_story_click",
      tile
    );
  }

  async fetchInferredPersonalizationImpression() {
    return await this.fetchInferredPersonalizationInteraction(
      "moz_newtab_story_impression"
    );
  }
  async fetchInferredPersonalizationClick() {
    return await this.fetchInferredPersonalizationInteraction(
      "moz_newtab_story_click"
    );
  }

  async recordInferredPersonalizationInteraction(table, tile) {
    const feature = tile.topic;
    const timestamp_s = this.Date().now() / 1000;
    const card_format_enum = FORMAT[tile.format];
    const position = tile.pos;
    const section_position = tile.section_position || 0;
    // TODO This needs to be attached to the tile, and coming from Merino.
    // TODO This is now in tile.features.
    // TODO It may be undefined if previous data was cached before Merino started returning features.
    const feature_value = 0.5;

    if (
      table !== "moz_newtab_story_impression" &&
      table !== "moz_newtab_story_click"
    ) {
      return;
    }

    await lazy.PlacesUtils.withConnectionWrapper(
      "newtab/lib/TelemetryFeed.sys.mjs: recordInferredPersonalizationImpression",
      async db => {
        await db.execute(
          `
          INSERT INTO ${table}(feature, timestamp_s, card_format_enum, position, section_position, feature_value)
          VALUES (:feature, :timestamp_s, :card_format_enum, :position, :section_position, :feature_value)
        `,
          {
            feature,
            timestamp_s,
            card_format_enum,
            position,
            section_position,
            feature_value,
          }
        );
      }
    );
  }

  async fetchInferredPersonalizationInteraction(table) {
    if (
      table !== "moz_newtab_story_impression" &&
      table !== "moz_newtab_story_click"
    ) {
      return [];
    }

    let sql = `SELECT feature, timestamp_s, card_format_enum, position, section_position, feature_value
    FROM ${table}`;
    //sql += `WHERE timestamp_s >= ${beginTimeSecs * 1000000}`;
    //sql += `AND timestamp_s < ${endTimeSecs * 1000000}`;

    const { activityStreamProvider } = lazy.NewTabUtils;
    const interactions = await activityStreamProvider.executePlacesQuery(sql);

    return interactions;
  }

  async onPrefChangedAction(action) {
    switch (action.data.name) {
      case PREF_USER_INFERRED_PERSONALIZATION:
      case PREF_SYSTEM_INFERRED_PERSONALIZATION:
        if (this.isEnabled() && action.data.value) {
          await this.loadInterestVector();
        } else {
          await this.reset();
        }
        break;
    }
  }

  async onAction(action) {
    switch (action.type) {
      case at.INIT:
        if (this.isEnabled()) {
          await this.init();
        }
        break;
      case at.UNINIT:
        await this.reset();
        break;
      case at.DISCOVERY_STREAM_DEV_SYSTEM_TICK:
      case at.SYSTEM_TICK:
        if (this.loaded && this.isEnabled()) {
          await this.loadInterestVector();
        }
        break;
      case at.INFERRED_PERSONALIZATION_REFRESH:
        if (this.loaded && this.isEnabled()) {
          await this.reset();
          await this.loadInterestVector();
        }
        break;
      case at.PLACES_HISTORY_CLEARED:
        // TODO Handle places history clear
        break;
      case at.DISCOVERY_STREAM_IMPRESSION_STATS:
        if (this.loaded && this.isEnabled()) {
          await this.handleDiscoveryStreamImpressionStats(action);
        }
        break;
      case at.DISCOVERY_STREAM_USER_EVENT:
        if (this.loaded && this.isEnabled()) {
          await this.handleDiscoveryStreamUserEvent(action);
        }
        break;
      case at.PREF_CHANGED:
        await this.onPrefChangedAction(action);
        break;
    }
  }
}

/**
 * Creating a thin wrapper around PersistentCache, and Date.
 * This makes it easier for us to write automated tests that simulate responses.
 */
InferredPersonalizationFeed.prototype.PersistentCache = (...args) => {
  return new lazy.PersistentCache(...args);
};
InferredPersonalizationFeed.prototype.Date = () => {
  return Date;
};
