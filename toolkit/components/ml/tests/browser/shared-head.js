/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  SmartTabGroupingManager,
  CLUSTER_METHODS,
  ANCHOR_METHODS,
  getBestAnchorClusterInfo,
} = ChromeUtils.importESModule(
  "moz-src:///browser/components/tabbrowser/SmartTabGrouping.sys.mjs"
);

/**
 * Checks if numbers are close up to decimalPoints decimal points
 *
 * @param {number} a
 * @param {number} b
 * @param {number} decimalPoints
 * @returns {boolean} True if numbers are similar
 */
function numberLooseEquals(a, b, decimalPoints = 2) {
  return a.toFixed(decimalPoints) === b.toFixed(decimalPoints);
}

/**
 * Compares two vectors up to decimalPoints decimal points
 * Returns true if all items the same up to decimalPoints threshold
 *
 * @param {number[]} a
 * @param {number[]} b
 * @param {number} decimalPoints
 * @returns {boolean} True if vectors are similar
 */
function vectorLooseEquals(a, b, decimalPoints = 2) {
  return a.every(
    (item, index) =>
      item.toFixed(decimalPoints) === b[index].toFixed(decimalPoints)
  );
}

/**
 * Extremely simple generator deterministic seeded list of numbers between
 * 0 and 1 for use of tests in place of a true random generator
 *
 * @param {number} seed
 * @returns {function(): number}
 */
function simpleNumberSequence(seed = 0) {
  const values = [
    0.42, 0.145, 0.5, 0.9234, 0.343, 0.1324, 0.8343, 0.534, 0.634, 0.3233,
  ];
  let counter = Math.floor(seed) % values.length;
  return () => {
    counter = (counter + 1) % values.length;
    return values[counter];
  };
}

/**
 * Utility function to shuffle an array, using a random
 *
 * @param {object[]} array of items to shuffle
 * @param {Function} randFunc function that returns between 0 and 1
 */
function shuffleArray(array, randFunc) {
  randFunc = randFunc ?? Math.random;
  for (let i = array.length - 1; i >= 0; i--) {
    const j = Math.floor(randFunc() * (i + 1));
    [array[i], array[j]] = [array[j], array[i]];
  }
}

/**
 * Returns dict that averages input values
 *
 * @param {object[]} itemArray List of dicts, each with values to average
 * @returns {object} Object with average of values passed in itemArray
 */
function averageStatsValues(itemArray) {
  const result = {};
  if (itemArray.length === 0) {
    return result;
  }
  for (const key of Object.keys(itemArray[0])) {
    let total = 0.0;
    itemArray.forEach(a => (total += a[key]));
    result[key] = total / itemArray.length;
  }
  return result;
}

/**
 * Read tsv file from string
 *
 * @param {string} tsvString string to read from
 * @returns {object} Object with parsed tsv string
 */
function parseTsvStructured(tsvString) {
  const rows = tsvString.trim().split("\n");
  const keys = rows[0].split("\t");
  const arrayOfDicts = rows.slice(1).map(row => {
    const values = row.split("\t");
    // Map keys to corresponding values
    const dict = {};
    keys.forEach((key, index) => {
      dict[key] = values[index];
    });
    return dict;
  });
  return arrayOfDicts;
}

/**
 * Read tsv string with embeddings
 *
 * @param {string} tsvString string with embeddings present
 * @returns {object} Object containing the embeddings
 */
function parseTsvEmbeddings(tsvString) {
  const rows = tsvString.trim().split("\n");
  return rows.map(row => {
    return row.split("\t").map(value => parseFloat(value));
  });
}

/**
 *
 * @param {string} clusterMethod kmeans or kmeans with anchor
 * @param {string} umapMethod umap or dbscan
 * @param {object[]} tabs tabs to cluster
 * @param {object[]} embeddings precomputed embeddings for the tabs
 * @param {number} iterations number of iterations before stopping clustering
 * @param {number[]} preGroupedTabIndices indices of tabs that are present in the group
 * @param {string} anchorMethod fixed or drift anchor methods
 * @param {number} silBoost what value to multiply silhouette score
 * @returns {Promise<{object}>} average of metric results
 */
async function testAugmentGroup(
  clusterMethod,
  umapMethod,
  tabs,
  embeddings,
  iterations = 1,
  preGroupedTabIndices,
  anchorMethod = ANCHOR_METHODS.FIXED,
  silBoost = undefined
) {
  const groupManager = new SmartTabGroupingManager();
  groupManager.setAnchorMethod(anchorMethod);
  if (silBoost !== undefined) {
    groupManager.setSilBoost(silBoost);
  }
  const randFunc = simpleNumberSequence();
  groupManager.setDataTitleKey("title");
  groupManager.setClusteringMethod(clusterMethod);
  groupManager.setDimensionReductionMethod(umapMethod);
  const allScores = [];
  for (let i = 0; i < iterations; i++) {
    const groupingResult = await groupManager.generateClusters(
      tabs,
      embeddings,
      0,
      randFunc,
      preGroupedTabIndices
    );
    const titleKey = "title";
    const centralClusterTitles = new Set(
      groupingResult.getAnchorCluster().tabs.map(a => a[titleKey])
    );
    groupingResult.getAnchorCluster().print();
    const anchorTitleSet = new Set(
      preGroupedTabIndices.map(a => tabs[a][titleKey])
    );
    Assert.equal(
      centralClusterTitles.intersection(anchorTitleSet).size,
      anchorTitleSet.size,
      `All anchor indices in target cluster`
    );
    const scoreInfo = groupingResult.getAccuracyStatsForCluster(
      "smart_group_label",
      groupingResult.getAnchorCluster().tabs[0].smart_group_label
    );
    allScores.push(scoreInfo);
  }
  return averageStatsValues(allScores);
}

/**
 * Runs clustering test with multiple anchor tabs
 *
 * @param {object[]} data tabs to run test on
 * @param {object []} precomputedEmbeddings embeddings for the tabs
 * @param {number[]} anchorGroupIndices indices of tabs already present in the group
 * @param {string} anchorMethod fixed or drift anchor method
 * @param {number} silBoost value with which to boost silhouette score
 * @returns {Promise<{}|null>} metric stats from running the clustering test
 */
async function runAnchorTabTest(
  data,
  precomputedEmbeddings = null,
  anchorGroupIndices,
  anchorMethod = ANCHOR_METHODS.FIXED,
  silBoost = undefined
) {
  const testParams = [[CLUSTER_METHODS.KMEANS]];
  let scoreInfo;
  for (let testP of testParams) {
    scoreInfo = await testAugmentGroup(
      testP[0],
      testP[1],
      data,
      precomputedEmbeddings,
      1,
      anchorGroupIndices,
      anchorMethod,
      silBoost
    );
  }
  if (testParams.length === 1) {
    return scoreInfo;
  }
  return null;
}

/**
 * Fetches a local file from prefix and filename
 *
 * @param {string} host_prefix root data folder path
 * @param {string} filename name of file
 * @returns {Promise}
 */
function fetchFile(host_prefix, filename) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    // const url = `${HOST_PREFIX}${filename}`;
    const url = `${host_prefix}${filename}`;
    xhr.open("GET", url, true);
    xhr.onload = () => {
      if (xhr.status === 200) {
        resolve(xhr.responseText);
      } else {
        reject(new Error(`Failed to fetch data: ${xhr.statusText}`));
      }
    };
    xhr.onerror = () => reject(new Error(`Network error getting ${url}`));
    xhr.send();
  });
}
