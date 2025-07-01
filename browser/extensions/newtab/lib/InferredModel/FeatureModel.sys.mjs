/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file has classes to combine New Tab feature events (aggregated from a sqlLite table) into an interest model.
 */

import {
  FORMAT,
  AggregateResultKeys,
  SPECIAL_FEATURE_CLICK,
} from "resource://newtab/lib/InferredModel/InferredConstants.sys.mjs";

export const DAYS_TO_MS = 60 * 60 * 24 * 1000;

const MAX_INT_32 = 2 ** 32;

/**
 * Divides numerator fields by the denominator. Value is set to 0 if denominator is missing or 0.
 * @param {Object.<string, number>} numerator
 * @param {Object.<string, number>} denominator
 * returns {Object.<string, number>}
 */
function divideDict(numerator, denominator) {
  const result = {};
  Object.keys(numerator).forEach(k => {
    result[k] = denominator[k] ? numerator[k] / denominator[k] : 0;
  });
  return result;
}

/**
 * Returns a secure random value between 0 and 1
 */
function secureRandomNumber() {
  const array = new Uint32Array(1);
  crypto.getRandomValues(array);
  return array[0] / MAX_INT_32;
}

/**
 * Applies laplace noise at a given scale
 * @param {number} scale value
 * @returns noisy value
 */
function laplaceNoise(scale) {
  const u = secureRandomNumber() - 0.5;
  return -scale * Math.sign(u) * Math.log(1 - 2 * Math.abs(u));
}

/**
 * Unary encoding with randomized response for differential privacy.
 * The output must be decoded to back to an integer when aggregating a historgram on a server
 * @param {number} x - Integer input (0 <= x < N)
 * @param {number} N - Number of values (see ablove)
 * @param {number} p - Probability of keeping a 1-bit as 1 (after one-hot encoding the output)
 * @param {number} q - Probability of flipping a 0-bit to 1
 * @returns {string} - Bitstring after unary encoding and randomized response
 */
export function unaryEncodeDiffPrivacy(x, N, p, q) {
  const bitstring = [];
  const randomValues = new Uint32Array(N);
  crypto.getRandomValues(randomValues);
  for (let i = 0; i < N; i++) {
    const trueBit = i === x ? 1 : 0;
    const rand = randomValues[i] / MAX_INT_32;
    if (trueBit === 1) {
      bitstring.push(rand <= p ? "1" : "0");
    } else {
      bitstring.push(rand <= q ? "1" : "0");
    }
  }
  return bitstring.join("");
}

/**
 * Adds value to all a particular key in a dictionary. If the key is missing it sets the value.
 * @param {Object} dict - The dictionary to modify.
 * @param {string} key - The key whose value should be added or set.
 * @param {number} value - The value to add to the key.
 */
export function dictAdd(dict, key, value) {
  if (key in dict) {
    dict[key] += value;
  } else {
    dict[key] = value;
  }
}

/**
 * Apply function to all keys in dictionary, returning new dictionary.
 * @param {Object} obj - The object whose values should be transformed.
 * @param {Function} fn - The function to apply to each value.
 * @returns {Object} A new object with the transformed values.
 */
export function dictApply(obj, fn) {
  return Object.fromEntries(
    Object.entries(obj).map(([key, value]) => [key, fn(value)])
  );
}

/**
 * Class for re-scaling events based on time passed.
 */
export class DayTimeWeighting {
  /**
   * Instantiate class based on a series of day periods in the past.
   * @param {int[]} pastDays Series of number of days, indicating days ago intervals in reverse chonological order.
   * Intervals are added: If the first value is 1 and the second is 5, then the first inteval is 0-1 and second interval is between 1 and 6.
   * @param {number[]} relativeWeight Relative weight of each period. Must be same length as pastDays
   */
  constructor(pastDays, relativeWeight) {
    this.pastDays = pastDays;
    this.relativeWeight = relativeWeight;
  }

  static fromJSON(json) {
    return new DayTimeWeighting(json.days, json.relative_weight);
  }

  /**
   * Get a series of interval pairs in the past based on the pastDays.
   * @param {number} curTimeMs Base time time in MS. Usually current time.
   * @returns
   */
  getDateIntervals(curTimeMs) {
    let curEndTime = curTimeMs;

    const res = this.pastDays.map(daysAgo => {
      const start = new Date(curEndTime - daysAgo * DAYS_TO_MS);
      const end = new Date(curEndTime);

      curEndTime = start;
      return { start, end };
    });
    return res;
  }

  /**
   * Get relative weight of current index.
   * @param {int} weightIndex Index
   * @returns {number} Weight at index, or 0 if index out of range.
   */
  getRelativeWeight(weightIndex) {
    if (weightIndex >= this.pastDays.length) {
      return 0;
    }
    return this.relativeWeight[weightIndex];
  }
}

/**
 * Describes the mapping from a set of aggregated events to a single interest feature
 */
export class InterestFeatures {
  constructor(
    name,
    featureWeights,
    thresholds = null,
    diff_p = 0.5,
    diff_q = 0.5
  ) {
    this.name = name;
    this.featureWeights = featureWeights;
    // Thresholds must be in ascending order
    this.thresholds = thresholds;
    this.diff_p = diff_p;
    this.diff_q = diff_q;
  }

  static fromJSON(name, json) {
    return new InterestFeatures(
      name,
      json.features,
      json.thresholds || null,
      json.diff_p,
      json.diff_q
    );
  }

  /**
   * Quantize a feature value based on the thresholds specified in the class.
   * @param {number} inValue Value computed by model for the feature.
   * @returns Quantized value. A value between 0 and number of thresholds specified (inclusive)
   */
  applyThresholds(inValue) {
    if (!this.thresholds) {
      return inValue;
    }
    for (let k = 0; k < this.thresholds.length; k++) {
      if (inValue < this.thresholds[k]) {
        return k;
      }
    }
    return this.thresholds.length;
  }

  /**
   * Applies Differential Privacy Unary Encoding method, outputting a one-hot encoded vector with randomizaiton.
   * Accurate historgrams of values can be computed with reasonable accuracy.
   * If the class has no or 0 p/q values set for differential privacy, then response is original number non-encoded.
   * @param {number} inValue Value to randomize
   * @returns Bitfield as a string, that is the same as the thresholds length + 1
   */
  applyDifferentialPrivacy(inValue) {
    if (!this.thresholds || !this.diff_p) {
      return inValue;
    }
    return unaryEncodeDiffPrivacy(
      inValue,
      this.thresholds.length + 1,
      this.diff_p,
      this.diff_q
    );
  }
}

/**
 * Manages relative tile importance
 */
export class TileImportance {
  constructor(tileImportanceMappings) {
    this.mappings = {};
    for (const [formatKey, formatId] of Object.entries(FORMAT)) {
      if (formatKey in tileImportanceMappings) {
        this.mappings[formatId] = tileImportanceMappings[formatKey];
      }
    }
  }

  getRelativeCTRForTile(tileType) {
    return this.mappings[tileType] || 1;
  }

  static fromJSON(json) {
    return new TileImportance(json);
  }
}

/***
 * A simple model for aggregating features
 */

export class FeatureModel {
  /**
   *
   * @param {string} modelId
   * @param {Object} dayTimeWeighting Data for day time weighting class
   * @param {Object} interestVectorModel Data for interest model
   * @param {Object} tileImportance Data for tile importance
   * @param {boolean} rescale Whether to rescale to max value
   * @param {boolean} logScale Whether to apply natural log (ln(x+ 1)) before rescaling
   */
  constructor({
    modelId,
    dayTimeWeighting,
    interestVectorModel,
    tileImportance,
    modelType,
    rescale = true,
    logScale = false,
    noiseScale = 0,
    laplaceNoiseFn = laplaceNoise,
  }) {
    this.modelId = modelId;
    this.tileImportance = tileImportance;
    this.dayTimeWeighting = dayTimeWeighting;
    this.interestVectorModel = interestVectorModel;
    this.rescale = rescale;
    this.logScale = logScale;
    this.modelType = modelType;
    this.noiseScale = noiseScale;
    this.laplaceNoiseFn = laplaceNoiseFn;
  }

  static fromJSON(json) {
    const dayTimeWeighting = DayTimeWeighting.fromJSON(json.day_time_weighting);
    const interestVectorModel = {};
    const tileImportance = TileImportance.fromJSON(json.tile_importance || {});

    for (const [name, featureJson] of Object.entries(json.interest_vector)) {
      interestVectorModel[name] = InterestFeatures.fromJSON(name, featureJson);
    }

    return new FeatureModel({
      dayTimeWeighting,
      tileImportance,
      interestVectorModel,
      normalize: json.normalize,
      rescale: json.rescale,
      logScale: json.log_scale,
      clickScale: json.clickScale,
      modelType: json.model_type,
      noiseScale: json.noise_scale,
    });
  }

  supportsCoarseInterests() {
    return Object.values(this.interestVectorModel).every(
      fm => fm.thresholds && fm.thresholds.length
    );
  }

  supportsCoarsePrivateInterests() {
    return Object.values(this.interestVectorModel).every(
      fm =>
        fm.thresholds &&
        fm.thresholds.length &&
        "diff_p" in fm &&
        "diff_q" in fm
    );
  }

  /**
   * Return date intervals for the query
   */
  getDateIntervals(curTimeMs) {
    return this.dayTimeWeighting.getDateIntervals(curTimeMs);
  }

  /**
   * Computes an interest vector or aggregate based on the model and raw sql inout.
   * @param {Object} config
   * @param {Array.<Array.<string|number>>} config.dataForIntervals Raw aggregate output from SQL query. Could be clicks or impressions
   * @param {Object.<string, number>} config.indexSchema Map of keys to indices in each sub-array in dataForIntervals
   * @param {boolean} [config.applyThresholding=false] Whether to apply thresholds
   * @param {boolean} [config.applyDifferntialPrivacy=false] Whether to apply differential privacy. This will be used for sending to telemetry.
   * @returns
   */
  computeInterestVector({
    dataForIntervals,
    indexSchema,
    applyThresholding = false,
    applyDifferentialPrivacy = false,
  }) {
    const processedPerTimeInterval = dataForIntervals.map(
      (intervalData, idx) => {
        const intervalRawTotal = {};
        const perPeriodTotals = {};
        intervalData.forEach(aggElement => {
          const feature = aggElement[indexSchema[AggregateResultKeys.FEATURE]];
          let value = aggElement[indexSchema[AggregateResultKeys.VALUE]]; // In the future we could support format here
          dictAdd(intervalRawTotal, feature, value);
        });

        const weight = this.dayTimeWeighting.getRelativeWeight(idx); // Weight for this time interval
        Object.values(this.interestVectorModel).forEach(interestFeature => {
          for (const featureUsed of Object.keys(
            interestFeature.featureWeights
          )) {
            if (featureUsed in intervalRawTotal) {
              dictAdd(
                perPeriodTotals,
                interestFeature.name,
                intervalRawTotal[featureUsed] *
                  weight *
                  interestFeature.featureWeights[featureUsed]
              );
            }
          }
        });
        return perPeriodTotals;
      }
    );

    // Since we are doing linear combinations, it is fine to do the day-time weighting at this step
    let totalResults = {};
    processedPerTimeInterval.forEach(intervalTotals => {
      for (const key of Object.keys(intervalTotals)) {
        dictAdd(totalResults, key, intervalTotals[key]);
      }
    });

    let numClicks = -1;

    // If clicks is a feature, it's handled as special case
    if (SPECIAL_FEATURE_CLICK in totalResults) {
      numClicks = totalResults[SPECIAL_FEATURE_CLICK];
      delete totalResults[SPECIAL_FEATURE_CLICK];
    }

    if (this.logScale) {
      totalResults = dictApply(totalResults, x => Math.log(x + 1));
    }

    if (this.rescale) {
      let divisor = Math.max(...Object.values(totalResults));
      if (divisor <= 0.001) {
        divisor = 0.001;
      }
      totalResults = dictApply(totalResults, x => x / divisor);
    }

    if (this.clickScale && numClicks > 0) {
      totalResults = dictApply(totalResults, x => x / numClicks);
    }

    if (numClicks >= 0) {
      totalResults[SPECIAL_FEATURE_CLICK] = numClicks;
    }

    if (applyThresholding) {
      if (applyDifferentialPrivacy) {
        // Zero values need to be shown so they can be randomized
        Object.values(this.interestVectorModel).forEach(interestFeature => {
          if (!(interestFeature.name in totalResults)) {
            totalResults[interestFeature.name] = 0;
          }
        });
      }
      for (const key of Object.keys(totalResults)) {
        if (key in this.interestVectorModel) {
          totalResults[key] = this.interestVectorModel[key].applyThresholds(
            totalResults[key],
            applyDifferentialPrivacy
          );
          if (applyDifferentialPrivacy) {
            totalResults[key] = this.interestVectorModel[
              key
            ].applyDifferentialPrivacy(
              totalResults[key],
              applyDifferentialPrivacy
            );
          }
        }
      }
    }
    return totalResults;
  }

  /**
   * Given pre-computed inferredInterests for clicks and impressions, returns a ctr result with
   * @param {Object} clickDict clicks dictionary
   * @param {Object} impressionDict impression dictionary
   * @param {String} model_id Model ID
   * @returns model
   */
  computeCTRInterestVectors(clickDict, impressionDict, model_id) {
    const inferredInterests = divideDict(clickDict, impressionDict);
    this.applyLaplaceNoise(inferredInterests);
    return { ...inferredInterests, model_id };
  }

  /**
   * Applies laplace noise to values in a dictionary if specified in the model
   * @param {Object} inputDict
   * @returns
   */
  applyLaplaceNoise(inputDict) {
    if (!this.noiseScale) {
      return;
    }
    for (const key in inputDict) {
      if (typeof inputDict[key] === "number") {
        const noise = this.laplaceNoiseFn(this.noiseScale);
        inputDict[key] += noise;
      }
    }
  }

  /**
   * Computes the interest vector for data intervals, as well as the coarse and privatized (with randomess)
   */
  computeInterestVectors({
    dataForIntervals,
    indexSchema,
    model_id = "unknown",
    condensePrivateValues = true,
  }) {
    const result = {};
    let inferredInterests;
    let coarseInferredInterests;
    let coarsePrivateInferredInterests;

    inferredInterests = this.computeInterestVector({
      dataForIntervals,
      indexSchema,
    });
    const updatedFuzzyInterests = { ...inferredInterests };
    this.applyLaplaceNoise(updatedFuzzyInterests);
    result.inferredInterests = { ...updatedFuzzyInterests, model_id };

    if (this.supportsCoarseInterests()) {
      coarseInferredInterests = this.computeInterestVector({
        dataForIntervals,
        indexSchema,
        applyThresholding: true,
      });
      if (coarseInferredInterests) {
        result.coarseInferredInterests = {
          ...coarseInferredInterests,
          model_id,
        };
      }
    }

    if (this.supportsCoarsePrivateInterests()) {
      coarsePrivateInferredInterests = this.computeInterestVector({
        dataForIntervals,
        indexSchema,
        applyThresholding: true,
        applyDifferentialPrivacy: true,
      });
      if (coarsePrivateInferredInterests) {
        if (condensePrivateValues) {
          result.coarsePrivateInferredInterests = {
            // Key order preserved in Gecko
            values: Object.values(coarsePrivateInferredInterests),
            model_id,
          };
        } else {
          result.coarsePrivateInferredInterests = {
            ...coarsePrivateInferredInterests,
            model_id,
          };
        }
      }
    }
    return result;
  }
}
