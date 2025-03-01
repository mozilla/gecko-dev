/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const HOST_PREFIX =
  "https://example.com/browser/browser/components/tabbrowser/test/browser/smarttabgrouping/data/";

const CLUSTERING_TEST_IDS = ["pgh_trip", "gen_set_2", "animal"];

const {
  getBestAnchorClusterInfo,
  SmartTabGroupingManager,
  CLUSTER_METHODS,
  DIM_REDUCTION_METHODS,
  ANCHOR_METHODS,
} = ChromeUtils.importESModule("resource:///modules/SmartTabGrouping.sys.mjs");

async function getGroupScore(
  clusterMethod,
  umapMethod,
  tabs,
  embeddings,
  iterations = 10
) {
  const groupManager = new SmartTabGroupingManager();
  const randFunc = simpleNumberSequence();
  groupManager.setDataTitleKey("title");
  groupManager.setClusteringMethod(clusterMethod);
  groupManager.setDimensionReductionMethod(umapMethod);
  let total_score = 0.0;
  for (let i = 0; i < iterations; i++) {
    const groupingResult = await groupManager.generateClusters(
      tabs,
      embeddings,
      0,
      randFunc
    );
    const score = groupingResult.getRandScore("smart_group_label");
    total_score += score;
  }
  return total_score / iterations;
}

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

async function runClusteringTest(data, precomputedEmbeddings = null) {
  if (!precomputedEmbeddings) {
    shuffleArray(data, simpleNumberSequence(0));
  }
  const testParams = [[CLUSTER_METHODS.KMEANS, null]];
  let score = 0;
  for (let testP of testParams) {
    score = await getGroupScore(
      testP[0],
      testP[1],
      data,
      precomputedEmbeddings
    );
    console.log(
      `${testP[0]} ${testP[1] || "No dim reduction."} Rand score is ${score}`
    );
  }
  if (testParams.length === 1) {
    return score;
  }
  console.warn(
    "Test checks on score not enabled because we are testing multiple methods"
  );
  return 1;
}

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
  console.warn(
    "Test checks on score not enabled because we are testing multiple methods"
  );
  return null;
}

async function setup({ disabled = false, prefs = [] } = {}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "All"],
      ["browser.ml.modelCacheTimeout", 1000],
      ...prefs,
    ],
  });
}

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

function parseTsvEmbeddings(tsvString) {
  const rows = tsvString.trim().split("\n");
  return rows.map(row => {
    return row.split("\t").map(value => parseFloat(value));
  });
}

function fetchFile(filename) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const url = `${HOST_PREFIX}${filename}`;
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

add_task(function testGetBestAnchorClusterInfo() {
  const { anchorClusterIndex, numAnchorItemsInCluster } =
    getBestAnchorClusterInfo(
      [
        [0, 1, 2],
        [3, 4],
        [5, 6, 7],
      ],
      [1, 2, 3]
    );
  Assert.equal(anchorClusterIndex, 0);
  Assert.equal(numAnchorItemsInCluster, 2);
});

add_task(function testAverageStats() {
  const res = averageStatsValues([{ a: 1 }, { a: 2 }, { a: 3 }]);
  Assert.equal(res.a, 2);
});

add_task(async function testClustering() {
  const testSets = CLUSTERING_TEST_IDS.map(test_id => [
    `${test_id}_embeddings.tsv`,
    `${test_id}_labels.tsv`,
  ]);
  for (const test of testSets) {
    const rawEmbeddings = await fetchFile(test[0]);
    const embeddings = parseTsvEmbeddings(rawEmbeddings);
    const rawLabels = await fetchFile(test[1]);
    const labels = parseTsvStructured(rawLabels);
    const score = await runClusteringTest(labels, embeddings);
    Assert.greater(score, 0.5, `Clustering ok for dataset ${test[0]}`);
  }
});

/**
 * Run tests for finding similar items for a single item or cluster of items with label anchorLabel
 * @param {Number[][]} embeddings Embeddings for each item
 * @param {String} anchorLabel String representing the ID of the anchor cluster
 * @param {Object[]} labels Dict representing each document (unused )
 * @param {String[]} labelClusterList List of items, with id of cluster for each (unused in)
 * @param {String} testName Name of the test dataset
 * @returns {Object[]}
 */
async function anchorTestsForCluster(
  embeddings,
  anchorLabel,
  documents,
  labelClusterList,
  testName,
  maxDocsToTest = 4
) {
  const scores = [];
  const possibleAnchorIndices = [];
  labelClusterList.forEach((cur, index) => {
    if (cur === anchorLabel) {
      possibleAnchorIndices.push(index);
    }
  });
  for (let numAnchors = 1; numAnchors < maxDocsToTest; numAnchors++) {
    shuffleArray(labelClusterList);
    const silBoost = 0.0;
    for (const anchorMethod of [ANCHOR_METHODS.FIXED, ANCHOR_METHODS.DRIFT]) {
      const anchorTestScoreInfo = await runAnchorTabTest(
        documents,
        embeddings,
        Array.from({ length: numAnchors }, (_, i) => i),
        anchorMethod,
        silBoost
      );
      scores.push({
        ...anchorTestScoreInfo,
        testName,
        anchorMethod,
        numAnchors,
        silBoost,
      });
    }
  }
  return scores;
}

add_task(async function testAnchorClustering() {
  const testSets = CLUSTERING_TEST_IDS.map(test_id => [
    `${test_id}_embeddings.tsv`,
    `${test_id}_labels.tsv`,
  ]);
  const LABEL_DICT_KEY = "smart_group_label";
  const scoreInfo = [];

  for (const test of testSets) {
    const rawEmbeddings = await fetchFile(test[0]);
    const embeddings = parseTsvEmbeddings(rawEmbeddings);
    const rawLabels = await fetchFile(test[1]);
    const labels = parseTsvStructured(rawLabels);
    const labelClusterList = labels.map(a => a[LABEL_DICT_KEY]);
    const uniqueLabels = Array.from(new Set(labelClusterList));

    /** Anchor Tests */
    const NUM_ANCHOR_LABELS_TO_TEST = 3;

    for (
      let labelIndex = 0;
      labelIndex < Math.max(uniqueLabels.length, NUM_ANCHOR_LABELS_TO_TEST);
      labelIndex++
    ) {
      const anchorLabel = uniqueLabels[labelIndex];
      const scoresForRuns = await anchorTestsForCluster(
        embeddings,
        anchorLabel,
        labels,
        labelClusterList,
        test[0]
      );
      scoreInfo.push(...scoresForRuns);
    }
  }

  const ACCEPTABLE_KAPPA = 0.15; // Quite a low bar, but have to start somewhere
  const percentageOfAcceptableKappa =
    scoreInfo.filter(a => a.kappa > ACCEPTABLE_KAPPA).length / scoreInfo.length;
  console.log(
    `${Math.round(percentageOfAcceptableKappa * 100)} % of tests greater than ${ACCEPTABLE_KAPPA} kappa`
  );
  Assert.greater(
    percentageOfAcceptableKappa,
    0.5,
    `Acceptable accuracy rates of anchor cluster operations`
  );
  // Print / log score info here when testing
  SimpleTest.waitForExplicitFinish();
});
