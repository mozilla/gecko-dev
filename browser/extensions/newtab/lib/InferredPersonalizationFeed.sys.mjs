/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  PersistentCache: "resource://newtab/lib/PersistentCache.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

import { FeatureModel } from "resource://newtab/lib/InferredModel/FeatureModel.sys.mjs";

import {
  FORMAT,
  AggregateResultKeys,
  DEFAULT_INFERRED_MODEL_DATA,
} from "resource://newtab/lib/InferredModel/InferredConstants.sys.mjs";

import {
  actionTypes as at,
  actionCreators as ac,
} from "resource://newtab/common/Actions.mjs";

import { MODEL_TYPE } from "./InferredModel/InferredConstants.sys.mjs";

const CACHE_KEY = "inferred_personalization_feed";
const DISCOVERY_STREAM_CACHE_KEY = "discovery_stream";
const INTEREST_VECTOR_UPDATE_TIME = 4 * 60 * 60 * 1000; // 4 hours
const PREF_USER_INFERRED_PERSONALIZATION =
  "discoverystream.sections.personalization.inferred.user.enabled";
const PREF_SYSTEM_INFERRED_PERSONALIZATION =
  "discoverystream.sections.personalization.inferred.enabled";
const PREF_SYSTEM_INFERRED_MODEL_OVERRIDE =
  "discoverystream.sections.personalization.inferred.model.override";

function timeMSToSeconds(timeMS) {
  return Math.round(timeMS / 1000);
}

const CLICK_TABLE = "moz_newtab_story_click";
const IMPRESSION_TABLE = "moz_newtab_story_impression";
const TEST_MODEL_ID = "TEST";

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

  async queryDatabaseForTimeIntervals(intervals, table) {
    let results = [];
    for (const interval of intervals) {
      const agg = await this.fetchInferredPersonalizationSummary(
        interval.start,
        interval.end,
        table
      );
      results.push(agg);
    }
    return results;
  }

  /**
   * Get Inferrred model raw data
   * @returns JSON of inferred model
   */
  async getInferredModelData() {
    const modelOverrideRaw =
      this.store.getState().Prefs.values[PREF_SYSTEM_INFERRED_MODEL_OVERRIDE];
    if (modelOverrideRaw) {
      if (modelOverrideRaw === TEST_MODEL_ID) {
        return {
          model_id: TEST_MODEL_ID,
          model_data: DEFAULT_INFERRED_MODEL_DATA,
        };
      }
      try {
        return JSON.parse(modelOverrideRaw);
      } catch (_error) {}
    }
    const dsCache = this.PersistentCache(DISCOVERY_STREAM_CACHE_KEY, true);
    const cachedData = (await dsCache.get()) || {};
    let { inferredModel } = cachedData;
    return inferredModel;
  }

  async generateInterestVector() {
    const inferredModel = await this.getInferredModelData();
    if (!inferredModel || !inferredModel.model_data) {
      return {};
    }
    const model = FeatureModel.fromJSON(inferredModel.model_data);

    const intervals = model.getDateIntervals(this.Date().now());
    const schema = {
      [AggregateResultKeys.FEATURE]: 0,
      [AggregateResultKeys.FORMAT_ENUM]: 1,
      [AggregateResultKeys.VALUE]: 2,
    };

    const aggClickPerInterval = await this.queryDatabaseForTimeIntervals(
      intervals,
      CLICK_TABLE
    );

    const interests = model.computeInterestVectors({
      dataForIntervals: aggClickPerInterval,
      indexSchema: schema,
      model_id: inferredModel.model_id,
    });

    if (model.modelType === MODEL_TYPE.CLICKS) {
      return interests;
    }

    if (
      model.modelType === MODEL_TYPE.CLICK_IMP_PAIR ||
      model.modelType === MODEL_TYPE.CTR
    ) {
      // This model type does not support differential privacy or thresholding
      const aggImpressionsPerInterval =
        await this.queryDatabaseForTimeIntervals(intervals, IMPRESSION_TABLE);
      const ivImpressions = model.computeInterestVector({
        dataForIntervals: aggImpressionsPerInterval,
        indexSchema: schema,
      });

      if (model.modelType === MODEL_TYPE.CTR) {
        const inferredInterests = model.computeCTRInterestVectors(
          interests.inferredInterests,
          ivImpressions,
          inferredModel.model_id
        );
        return { inferredInterests };
      }
      const res = {
        c: interests.inferredInterests,
        i: ivImpressions,
        model_id: inferredModel.model_id,
      };
      return { inferredInterests: res };
    }

    // unsupported modelType
    return {};
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
          inferredInterests: interest_vector.data.inferredInterests,
          coarseInferredInterests: interest_vector.data.coarseInferredInterests,
          coarsePrivateInferredInterests:
            interest_vector.data.coarsePrivateInferredInterests,
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
    await this.recordInferredPersonalizationInteraction(IMPRESSION_TABLE, tile);
  }
  async recordInferredPersonalizationClick(tile) {
    await this.recordInferredPersonalizationInteraction(
      CLICK_TABLE,
      tile,
      true
    );
  }

  async fetchInferredPersonalizationImpression() {
    return await this.fetchInferredPersonalizationInteraction(
      "moz_newtab_story_impression"
    );
  }

  async fetchInferredPersonalizationSummary(startTime, endTime, table) {
    let sql = `SELECT feature, card_format_enum, SUM(feature_value) FROM ${table}
      WHERE timestamp_s > ${timeMSToSeconds(startTime)}
      AND timestamp_s < ${timeMSToSeconds(endTime)}
       GROUP BY feature, card_format_enum`;
    const { activityStreamProvider } = lazy.NewTabUtils;
    const interactions = await activityStreamProvider.executePlacesQuery(sql);
    return interactions;
  }

  async recordInferredPersonalizationInteraction(
    table,
    tile,
    extraClickEvent = false
  ) {
    const timestamp_s = timeMSToSeconds(this.Date().now());
    const card_format_enum = FORMAT[tile.format];
    const position = tile.pos;
    const section_position = tile.section_position || 0;
    let featureValuePairs = [];
    if (extraClickEvent) {
      featureValuePairs.push(["click", 1]);
    }
    if (tile.features) {
      featureValuePairs = featureValuePairs.concat(
        Object.entries(tile.features)
      );
    }
    if (table !== CLICK_TABLE && table !== IMPRESSION_TABLE) {
      return;
    }
    const primaryValues = {
      timestamp_s,
      card_format_enum,
      position,
      section_position,
    };

    const insertValues = featureValuePairs.map(pair =>
      Object.assign({}, primaryValues, {
        feature: pair[0],
        feature_value: pair[1],
      })
    );

    let sql = `
    INSERT INTO ${table}(feature, timestamp_s, card_format_enum, position, section_position, feature_value)
    VALUES (:feature, :timestamp_s, :card_format_enum, :position, :section_position, :feature_value)
    `;
    await lazy.PlacesUtils.withConnectionWrapper(
      "newtab/lib/InferredPersonalizationFeed.sys.mjs: recordInferredPersonalizationImpression",
      async db => {
        await db.execute(sql, insertValues);
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
