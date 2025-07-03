/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module has utility functions for doing thompson sampling and ranking
 */

/**
 * Utility function to sort items based on a Thompson Sampling draw
 *
 * @param {object} observationsPriors - An object containing counts and priors for clicks and impressions
 * @param {int[]} observationsPriors.key_array - Array of items to be ranked
 * @param {int[]} observationsPriors.obs_positive - Array of clicks
 * @param {int[]} observationsPriors.obs_negative - Array of impressions
 * @param {int[]} observationsPriors.prior_positive - Array of priors for clicks
 * @param {int[]} observationsPriors.prior_negative - Array of priors for impressions
 * @param {boolean} observationsPriors.do_sort - Boolean flag for sorting scores and key_array
 * @returns {[accuracy: final_keys, kappa: final_thetas]} An object containing arrays of keys and scores
 */
export async function thompsonSampleSort({
  key_array,
  obs_positive,
  obs_negative,
  prior_positive,
  prior_negative,
  do_sort = true,
}) {
  // If priors are not provided, initialize them to arrays of 1s
  prior_positive = prior_positive ?? obs_positive.map(() => 1);
  prior_negative = prior_negative ?? obs_negative.map(() => 1);

  // sample a theta (score) for each item
  const thetas = key_array.map((_, i) =>
    sampleBeta(
      obs_positive[i] + prior_positive[i],
      obs_negative[i] + prior_negative[i]
    )
  );
  // sort theta and key_array by theta
  let final_keys, final_thetas;
  if (do_sort) {
    [final_keys, final_thetas] = sortKeysValues(thetas, key_array);
  } else {
    final_keys = key_array;
    final_thetas = thetas;
  }
  return [final_keys, final_thetas];
}

/**
 * Sort an array of keys by values in scores
 *
 * @param {number[]} scores The vector to with values we sort by
 * @param {object[]} keys Array of keys to be sorted
 * @returns {[number[], object[] ]} Sorted keys and sorted scores
 */
export function sortKeysValues(scores, keys) {
  // Pair the values
  const paired = scores.map((score, i) => ({ score, key: keys[i] }));

  // Sort by score descending
  paired.sort((a, b) => b.score - a.score);

  // Unzip into separate arrays
  const sortedScores = paired.map(p => p.score);
  const sortedKeys = paired.map(p => p.key);

  return [sortedKeys, sortedScores];
}

/**
 * Sample from a Beta distribution, only valid for a and b >=1
 *
 * @param {number} a Alpha in the Beta distribution
 * @param {number} b Beta in the Beta distribution
 * @returns {number } A sampled float from a Beta distribution
 */
export function sampleBeta(a, b) {
  const ag = sampleGamma(a);
  const bg = sampleGamma(b);
  return ag / (ag + bg);
}

/**
 * Sample from a Gamma distribution, only valid for a>=1
 *
 * @param {number} a Shape of the Gamma distribution
 * @param {object} normalSampler Helper function for making tests deterministic
 * @param {object} uniSampler Helper function for making tests deterministic
 * @returns {number } A sampled float from a Gamma distribution
 */
export function sampleGamma(
  a,
  normalSampler = sampleNormal,
  uniSampler = Math.random
) {
  // Marsaglia and Tsang method for sampling from gamma
  // requires a > 1 to be valid! there are other methods for a < 1 to be implemented
  let x, v, uni;
  const d = a - 1 / 3;
  const c = 1 / Math.sqrt(9 * d);
  do {
    x = normalSampler();
    v = Math.pow(1 + c * x, 3);
    uni = uniSampler();
  } while (
    v < 0 ||
    Math.log(uni) > 0.5 * Math.pow(x, 2) + d - d * v + d * Math.log(v)
  );
  return d * v;
}

/**
 * Sample from a Normal distribution
 *
 * @param {object} getRandom Helper function for making tests deterministic
 * @returns {number } A sampled float from a Gamma distribution
 */
export function sampleNormal(getRandom = Math.random) {
  // Magic constants below are straight from Leva paper
  // These are left as variables to better match Leva paper
  const s = 0.449871;
  const t = -0.386595;
  const a = 0.196;
  const b = 0.25472;

  let u, v, x, y, q;
  while (true) {
    u = getRandom();
    v = 1.7156 * (getRandom() - 0.5);
    x = u - s;
    y = Math.abs(v) - t;
    q = Math.pow(x, 2) + y * (a * y - b * x);

    if (q < 0.27597) {
      break;
    }
    if (q > 0.27846) {
      continue;
    }
    if (v * v <= -4 * Math.log(u) * u * u) {
      break;
    }
  }

  return v / u;
}
