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
          unit: "MiB",
          shouldAlert: true,
        },
        {
          name: "tokenSpeed",
          unit: "tokens/s",
          shouldAlert: true,
          lowerIsBetter: false,
        },
        {
          name: "charactersSpeed",
          unit: "chars/s",
          shouldAlert: true,
          lowerIsBetter: false,
        },
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

requestLongerTimeout(250);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Topic model tests
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

  await perfTest({
    name: "smart-tab-topic",
    options,
    request,
    iterations: 2,
    addColdStart: true,
  });
});

// Embedding model tests
async function testEmbedding(trackPeakMemory = false) {
  const options = new PipelineOptions({
    taskName: "feature-extraction",
    modelId: "Mozilla/smart-tab-embedding",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
    timeoutMS: 2 * 60 * 1000,
  });

  var input;

  if (trackPeakMemory) {
    input = [
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
      "Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact",
      "The Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact on tech.",
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
      "Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact",
      "The Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact on tech.",
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
      "Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact",
      "The Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact on tech.",
    ];
  } else {
    input = [
      "Vintage Cameras and Their Impact The First Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections",
      "Personal Computers Discussing the Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact",
      "The Evolution of Smartphones 80s Tech Recollections Vintage Cameras and Their Impact on tech.",
    ];
  }

  const requestInfo = {
    inputArgs: input,
    runOptions: {
      pooling: "mean",
      normalize: true,
    },
  };

  const request = {
    args: [requestInfo.inputArgs],
    options: requestInfo.runOptions,
  };

  await perfTest({
    name: "smart-tab-embedding",
    options,
    request,
    iterations: 2,
    addColdStart: true,
    trackPeakMemory,
    peakMemoryInterval: 10,
  });
}

add_task(async function test_ml_smart_tab_embedding() {
  await testEmbedding(false);
});

// Clustering / Nearest Neighbor tests
const ROOT_URL =
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data/tab_grouping/";

/*
 * Generate n random samples by loading existing labels and embeddings
 */
function generateSamples(labels, embeddings, n) {
  let generatedLabels = [];
  let generatedEmbeddings = [];
  for (let i = 0; i < n; i++) {
    const randomIndex = Math.floor(Math.random() * labels.length);
    generatedLabels.push(labels[randomIndex]);
    if (embeddings) {
      generatedEmbeddings.push(embeddings[randomIndex]);
    }
  }
  return {
    labels: generatedLabels,
    embeddings: generatedEmbeddings,
  };
}

async function generateEmbeddings(textList) {
  const options = new PipelineOptions({
    taskName: "feature-extraction",
    modelId: "Mozilla/smart-tab-embedding",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
    timeoutMS: 2 * 60 * 1000,
  });
  const requestInfo = {
    inputArgs: textList,
    runOptions: {
      pooling: "mean",
      normalize: true,
    },
  };

  const request = {
    args: [requestInfo.inputArgs],
    options: requestInfo.runOptions,
  };
  const mlEngineParent = await EngineProcess.getMLEngineParent();
  const engine = await mlEngineParent.getEngine(options);
  const output = await engine.run(request);
  return output;
}

const singleTabMetrics = {};
singleTabMetrics["SINGLE-TAB-LATENCY"] = [];

add_task(async function test_clustering() {
  const modelHubRootUrl = Services.env.get("MOZ_MODELS_HUB");
  const { cleanup } = await perfSetup({
    prefs: [["browser.ml.modelHubRootUrl", modelHubRootUrl]],
  });

  const stgManager = new SmartTabGroupingManager();

  let generateEmbeddingsStub = sinon.stub(
    SmartTabGroupingManager.prototype,
    "_generateEmbeddings"
  );
  generateEmbeddingsStub.callsFake(async textList => {
    return await generateEmbeddings(textList);
  });

  const labelsPath = `gen_set_2_labels.tsv`;
  const rawLabels = await fetchFile(ROOT_URL, labelsPath);
  let labels = parseTsvStructured(rawLabels);
  labels = labels.map(l => ({ ...l, label: l.smart_group_label }));
  const startTime = performance.now();
  const similarTabs = await stgManager.findNearestNeighbors(
    labels,
    [1],
    [],
    0.3
  );
  const endTime = performance.now();
  singleTabMetrics["SINGLE-TAB-LATENCY"].push(endTime - startTime);
  const titles = similarTabs.map(s => s.label);
  Assert.equal(
    titles.length,
    5,
    "Proper number of similar tabs should be returned"
  );
  Assert.equal(
    titles[0],
    "Impact of Tourism on Local Communities - Google Scholar"
  );
  Assert.equal(
    titles[1],
    "Tourist Behavior and Decision Making: A Research Overview"
  );
  Assert.equal(titles[2], "Global Health Outlook - Reuters");
  Assert.equal(titles[3], "Climate Change Impact 2022 - Google Scholar");
  Assert.equal(titles[4], "Hotel Deals: Save Big on Hotels with Expedia");
  reportMetrics(singleTabMetrics);
  generateEmbeddingsStub.restore();
  await EngineProcess.destroyMLEngine();
  await cleanup();
});

const N_TABS = [10];
const methods = ["KMEANS_ANCHOR", "NEAREST_NEIGHBORS_ANCHOR"];
const nTabMetrics = {};

for (let method of methods) {
  for (let n of N_TABS) {
    if (method === "KMEANS_ANCHOR" && n > 25) {
      break;
    }
    nTabMetrics[`${method}-${n}-TABS-latency`] = [];
  }
}

add_task(async function test_n_clustering() {
  const modelHubRootUrl = Services.env.get("MOZ_MODELS_HUB");
  const { cleanup } = await perfSetup({
    prefs: [["browser.ml.modelHubRootUrl", modelHubRootUrl]],
  });

  const stgManager = new SmartTabGroupingManager();

  let generateEmbeddingsStub = sinon.stub(
    SmartTabGroupingManager.prototype,
    "_generateEmbeddings"
  );
  generateEmbeddingsStub.callsFake(async textList => {
    return await generateEmbeddings(textList);
  });

  const labelsPath = `gen_set_2_labels.tsv`;
  const rawLabels = await fetchFile(ROOT_URL, labelsPath);
  const labels = parseTsvStructured(rawLabels);

  for (let n of N_TABS) {
    for (let method of methods) {
      for (let i = 0; i < 1; i++) {
        const samples = generateSamples(labels, null, n);
        let startTime = performance.now();
        if (method === "KMEANS_ANCHOR" && n <= 50) {
          await stgManager.generateClusters(
            samples.labels,
            null,
            0,
            null,
            [0],
            []
          );
        } else if (method === "NEAREST_NEIGHBORS_ANCHOR") {
          await stgManager.findNearestNeighbors(samples.labels, [0], []);
        }
        let endTime = performance.now();
        const key = `${method}-${n}-TABS-latency`;
        if (key in nTabMetrics) {
          nTabMetrics[key].push(endTime - startTime);
        }
        await EngineProcess.destroyMLEngine();
      }
    }
  }
  reportMetrics(nTabMetrics);
  generateEmbeddingsStub.restore();
  await cleanup();
});
