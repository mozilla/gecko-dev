/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const FORMAT_ENUM = {
  SMALL: 0,
  MEDIUM: 1,
  LARGE: 2,
};

export const FORMAT = {
  "small-card": FORMAT_ENUM.SMALL,
  "medium-card": FORMAT_ENUM.MEDIUM,
  "large-card": FORMAT_ENUM.LARGE,
};

/**
 * We are exploring two options for interest vectors
 */
export const MODEL_TYPE = {
  // Returns clicks and impressions as separate dictionaries
  CLICK_IMP_PAIR: "click_impression_pair",
  // Returns a single clicks dictionary, along with the total number of clicks
  CLICKS: "clicks",
  CTR: "ctr",
};

export const CLICK_FEATURE = "click";

export const AggregateResultKeys = {
  POSITION: "position",
  FEATURE: "feature",
  VALUE: "feature_value",
  SECTION_POSITION: "section_position",
  FORMAT_ENUM: "card_format_enum",
};

// Clicks feature is handled in certain ways by the model
export const SPECIAL_FEATURE_CLICK = "clicks";

export const DEFAULT_INFERRED_MODEL_DATA = {
  model_type: MODEL_TYPE.CLICKS,
  rescale: true,
  day_time_weighting: {
    days: [3, 14, 45],
    relative_weight: [1, 1, 1],
  },
  interest_vector: {
    parenting: {
      features: { parenting: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    arts: {
      features: { arts: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    health: {
      features: { arts: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    sports: {
      features: { sports: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    society: {
      features: { society: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    education: {
      features: { education: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    government: {
      features: { government: 1 },
      thresholds: [0.3, 0.4],
      diff_p: 0.75,
      diff_q: 0.25,
    },
    [SPECIAL_FEATURE_CLICK]: {
      features: { click: 1 },
      thresholds: [2, 8, 40],
      diff_p: 0.9,
      diff_q: 0.1,
    },
  },
};

export const DEFAULT_INFERRED_MODEL = {
  model_id: "default",
  model_data: DEFAULT_INFERRED_MODEL_DATA,
};
