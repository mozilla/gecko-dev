/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Smart Tab Model",
  description: "Testing Smart Tab Models",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "memory",
          unit: "MB",
          shouldAlert: true,
        },
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

requestLongerTimeout(120);

// Helper functions for clustering test
const ROOT_URL =
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data/tab_grouping";

const CLUSTERING_TEST_IDS = ["pgh_trip", "gen_set_2", "animal"];

const { SmartTabGroupingManager, CLUSTER_METHODS } = ChromeUtils.importESModule(
  "resource:///modules/SmartTabGrouping.sys.mjs"
);

async function readFs(inputDataPath) {
  const response = await fetch(inputDataPath);
  if (!response.ok) {
    throw new Error(
      `Failed to fetch data: ${response.statusText} from ${inputDataPath}`
    );
  }
  return response.text();
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

async function runClusteringTest(data, precomputedEmbeddings = null) {
  if (!precomputedEmbeddings) {
    shuffleArray(data, seededRandomGenerator(0));
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
  }
  if (testParams.length === 1) {
    return score;
  }
  return 1;
}

async function getGroupScore(
  clusterMethod,
  umapMethod,
  tabs,
  embeddings,
  iterations = 10
) {
  const groupManager = new SmartTabGroupingManager();
  const randFunc = seededRandomGenerator();
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

function seededRandomGenerator(seed = 0) {
  const values = [
    0.42, 0.145, 0.5, 0.9234, 0.343, 0.1324, 0.8343, 0.534, 0.634, 0.3233,
  ];
  let counter = Math.floor(seed) % values.length;
  return () => {
    counter = (counter + 1) % values.length;
    return values[counter];
  };
}
function shuffleArray(array, randFunc) {
  randFunc = randFunc || Math.random;
  for (let i = array.length - 1; i >= 0; i--) {
    const j = Math.floor(randFunc() * (i + 1));
    [array[i], array[j]] = [array[j], array[i]];
  }
}

async function runAnchorTabTest(
  data,
  precomputedEmbeddings = null,
  anchorGroupIndices
) {
  const testParams = [[CLUSTER_METHODS.KMEANS, null]];
  let score = 0;
  for (let testP of testParams) {
    score = await testAugmentGroup(
      testP[0],
      testP[1],
      data,
      precomputedEmbeddings,
      10,
      anchorGroupIndices
    );
    console.log(
      `${testP[0]} ${testP[1] || "no dim reduction."} Anchor Items ${
        anchorGroupIndices.length
      } Rand score is ${score}`
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

async function testAugmentGroup(
  clusterMethod,
  umapMethod,
  tabs,
  embeddings,
  iterations = 3,
  preGroupedTabIndices
) {
  const groupManager = new SmartTabGroupingManager();
  const randFunc = seededRandomGenerator();
  groupManager.setDataTitleKey("title");
  groupManager.setClusteringMethod(clusterMethod);
  groupManager.setDimensionReductionMethod(umapMethod);
  let total_score = 0.0;
  for (let i = 0; i < iterations; i++) {
    const groupingResult = await groupManager.generateClusters(
      tabs,
      embeddings,
      0,
      randFunc,
      preGroupedTabIndices
    );
    const score = groupingResult.getRandScore("smart_group_label");
    total_score += score;
  }
  return total_score / iterations;
}

/*
 * Generate n random samples by loading existing labels and embeddings
 */
function generateSamples(labels, embeddings, n) {
  let generatedLabels = [];
  let generatedEmbeddings = [];
  for (let i = 0; i < n; i++) {
    const randomIndex = Math.floor(Math.random() * labels.length);
    generatedLabels.push(labels[randomIndex]);
    generatedEmbeddings.push(embeddings[randomIndex]);
  }
  return {
    labels: generatedLabels,
    embeddings: generatedEmbeddings,
  };
}

/**
 * Tests local Autofill model
 */
add_task(async function test_ml_smart_tab_topic() {
  const options = new PipelineOptions({
    taskName: "text2text-generation",
    modelId: "Mozilla/smart-tab-topic",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
    timeoutMS: 2 * 60 * 1000,
  });

  const requestInfo = {
    inputArgs:
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
    runOptions: {
      max_length: 6,
    },
  };

  const request = {
    args: [requestInfo.inputArgs],
    options: requestInfo.runOptions,
  };

  await perfTest("smart-tab-topic", options, request, ITERATIONS, true);
});

add_task(async function test_ml_smart_tab_embedding() {
  const options = new PipelineOptions({
    taskName: "feature-extraction",
    modelId: "Mozilla/smart-tab-embedding",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
    timeoutMS: 2 * 60 * 1000,
  });

  const requestInfo = {
    inputArgs: [
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
      "Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact",
      "The Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact on tech.",
    ],
    runOptions: {
      pooling: "mean",
      normalize: true,
    },
  };

  const request = {
    args: [requestInfo.inputArgs],
    options: requestInfo.runOptions,
  };

  await perfTest("smart-tab-embedding", options, request, ITERATIONS, true);
});

const KMEANS_CLUSTERING = "KMEANS_CLUSTERING";
const KMEANS_ANCHOR_TAB = "KMEANS_ANCHOR_TAB";
const clusteringMetrics = {};
for (let test_id of CLUSTERING_TEST_IDS) {
  for (let type of [KMEANS_CLUSTERING, KMEANS_ANCHOR_TAB]) {
    clusteringMetrics[`${type}-${test_id}-latency`] = [];
  }
}

add_task(async function test_clustering() {
  const testSets = CLUSTERING_TEST_IDS.map(testId => [
    `${ROOT_URL}/${testId}_embeddings.tsv`,
    `${ROOT_URL}/${testId}_labels.tsv`,
    testId,
  ]);
  for (const test of testSets) {
    const embeddingsPath = test[0];
    const labelsPath = test[1];
    const testId = test[2];
    const rawEmbeddings = await readFs(embeddingsPath);
    const embeddings = parseTsvEmbeddings(rawEmbeddings);
    const rawLabels = await readFs(labelsPath);
    const labels = parseTsvStructured(rawLabels);
    for (let i = 0; i < ITERATIONS; i++) {
      let startTime = performance.now();
      let score = await runClusteringTest(labels, embeddings);
      let endTime = performance.now();
      Assert.greater(score, 0.5, `Clustering ok for dataset ${test[0]}`);
      clusteringMetrics[`${KMEANS_CLUSTERING}-${testId}-latency`].push(
        endTime - startTime
      );
    }
    for (let i = 0; i < ITERATIONS; i++) {
      let startTime = performance.now();
      const anchorScore = await runAnchorTabTest(labels, embeddings, [0]);
      let endTime = performance.now();
      Assert.greater(
        anchorScore,
        0.5,
        `Single Anchor Clustering ok for dataset ${test[0]}`
      );
      clusteringMetrics[`${KMEANS_ANCHOR_TAB}-${testId}-latency`].push(
        endTime - startTime
      );
    }
  }
  reportMetrics(clusteringMetrics);
});

const N_TABS = [1, 2, 4, 8, 16, 32, 64, 128, 256];
const methods = ["KMEANS"];
const nTabMetrics = {};

for (let method of methods) {
  for (let n of N_TABS) {
    nTabMetrics[`${method}-${n}-TABS-latency`] = [];
  }
}

add_task(async function test_n_clustering() {
  const embeddingsPath = `${ROOT_URL}/pgh_trip_embeddings.tsv`;
  const labelsPath = `${ROOT_URL}/pgh_trip_labels.tsv`;

  const rawEmbeddings = await readFs(embeddingsPath);
  const embeddings = parseTsvEmbeddings(rawEmbeddings);
  const rawLabels = await readFs(labelsPath);
  const labels = parseTsvStructured(rawLabels);

  for (let method of methods) {
    for (let n of N_TABS) {
      for (let i = 0; i < 2; i++) {
        const samples = generateSamples(labels, embeddings, n);
        let startTime = performance.now();
        await runAnchorTabTest(samples.labels, samples.embeddings, [0]);
        let endTime = performance.now();
        nTabMetrics[`${method}-${n}-TABS-latency`].push(endTime - startTime);
      }
    }
  }
  reportMetrics(nTabMetrics);
  Assert.ok(true);
});
