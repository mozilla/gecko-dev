/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  thompsonSampleSort: "chrome://global/content/ml/ThomSample.sys.mjs",
  sortKeysValues: "chrome://global/content/ml/ThomSample.sys.mjs",
});

const SHORTCUT_TABLE = "moz_newtab_shortcuts_interaction";
const PLACES_TABLE = "moz_places";

/**
 * Get clicks and impressions for sites in topsites array
 *
 * @param {object[]} topsites Array of topsites objects
 * @param {string} table Table for shortcuts interactions
 * @param {string} placeTable moz_places table
 * @returns {clicks: [number[], impressions: number[]]} Clicks and impressions for each site in topsites
 */
async function fetchShortcutInteractions(topsites, table, placeTable) {
  if (!topsites.length) {
    // Return empty clicks and impressions arrays
    return [[], []];
  }

  const guidList = topsites.map(site => site.guid);

  const valuesClause = guidList
    .map(guid => `('${guid.replace(/'/g, "''")}')`)
    .join(", ");

  // Only get records in the last month!
  // Join no places table to map guid to place_id
  const sql = `
    WITH input_keys(guid) AS (
      VALUES ${valuesClause}
    ),
    place_ids AS (
      SELECT input_keys.guid, ${placeTable}.id AS place_id
      FROM input_keys
      JOIN ${placeTable} ON ${placeTable}.guid = input_keys.guid
    )
    SELECT
      place_ids.guid AS key,
      COALESCE(SUM(${table}.event_type), 0) AS total_clicks,
      COALESCE(SUM(1 - ${table}.event_type), 0) AS total_impressions
    FROM place_ids
    LEFT JOIN ${table} ON ${table}.place_id = place_ids.place_id
      AND ${table}.timestamp_s >= strftime('%s', 'now', '-2 month')
    GROUP BY place_ids.guid;
  `;

  const { activityStreamProvider } = lazy.NewTabUtils;
  const interactions = await activityStreamProvider.executePlacesQuery(sql);
  const interactionMap = new Map(
    interactions.map(row => {
      // Destructure the array into variables
      const [key, total_clicks, total_impressions] = row;
      return [key, { clicks: total_clicks, impressions: total_impressions }];
    })
  );

  // Rebuild aligned arrays in same order as input
  const clicks = guidList.map(guid =>
    interactionMap.has(guid) ? interactionMap.get(guid).clicks : 0
  );

  const impressions = guidList.map(guid =>
    interactionMap.has(guid) ? interactionMap.get(guid).impressions : 0
  );
  return [clicks, impressions];
}

/**
 * Scale an array to have min value 0 and max value 1
 *
 * @param {number[]} arr Array to scale
 * @returns {scaled: number[]}  Scaled array
 */
function zeroOneScaler(arr) {
  const min = Math.min(...arr);
  const max = Math.max(...arr);
  const range = max - min;
  const scaled = arr.map(x => (x - min) / range);
  return scaled;
}

function multi_ranker(key_arr, arrs, weights) {
  // zero-one scale array, then weight
  const scaled_arrs = arrs.map((arr, i) =>
    zeroOneScaler(arr).map(num => num * weights[i])
  );
  // sum over scaled-weighted arrays
  const resolved_scores = scaled_arrs.reduce((sum, arr) =>
    arr.map((value, i) => sum[i] + value)
  );
  // sort keys by scores
  const [final_keys, final_scores] = lazy.sortKeysValues(
    resolved_scores,
    key_arr
  );
  return [final_keys, final_scores];
}

/**
 * Apply thompson sampling to topsites array, considers frecency weights
 *
 * @param {object[]} topsites Array of topsites objects
 * @param {number} alpha Positive prior applied to all topsites
 * @param {number} beta Negative prior applied to all topsites
 * @param {number} thom_weight Number between 0 and 1, weight for thompson sampled scores. Frecency weight will be the compliment
 * @returns {combined: object[]} Array of topsites in reranked order
 */
export async function tsampleTopSites(
  topsites,
  alpha = 1,
  beta = 1,
  thom_weight = 1.0
) {
  // split topsites into two arrays
  const [withGuid, withoutGuid] = topsites.reduce(
    ([withG, withoutG], site) => {
      if (site.guid && typeof site.guid === "string") {
        withG.push(site);
      } else {
        withoutG.push(site);
      }
      return [withG, withoutG];
    },
    [[], []]
  );
  // query for interactions
  const [clicks, impressions] = await fetchShortcutInteractions(
    withGuid,
    SHORTCUT_TABLE,
    PLACES_TABLE
  );

  // sample a sorted version of topsites
  const ranked_thetas = await lazy.thompsonSampleSort({
    key_array: withGuid,
    obs_positive: clicks,
    obs_negative: impressions,
    prior_positive: clicks.map(() => alpha),
    prior_negative: impressions.map(() => beta),
    do_sort: false,
  });

  // get frecency from withGUID
  const frec_scores = withGuid.map(site => site.frecency);

  // rank by frecency and thompson theta
  const ranked_scores = multi_ranker(
    withGuid,
    [frec_scores, ranked_thetas[1]],
    [1 - thom_weight, thom_weight]
  );

  // drop theta from ranked to keep just the keys, combine back
  const combined = ranked_scores[0].concat(withoutGuid);
  return combined; // returns keys ordered by sampled score
}
