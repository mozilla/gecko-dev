/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Suggest Feature",
  description: "Template test for latency for ML suggest Feature",
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
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

requestLongerTimeout(120);
const CUSTOM_INTENT_OPTIONS = {
  taskName: "text-classification",
  featureId: "suggest-intent-classification",
  engineId: "ml-suggest-intent",
  modelId: "Mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier",
  modelHubUrlTemplate: "{model}/{revision}",
  dtype: "q8",
  modelRevision: "main",
  numThreads: 2,
  timeoutMS: -1,
};

const CUSTOM_NER_OPTIONS = {
  taskName: "token-classification",
  featureId: "suggest-NER",
  engineId: "ml-suggest-ner",
  modelId: "Mozilla/distilbert-uncased-NER-LoRA",
  modelHubUrlTemplate: "{model}/{revision}",
  dtype: "q8",
  modelRevision: "main",
  numThreads: 2,
  timeoutMS: -1,
};

const journal = {};
const runInference2 = async () => {
  ChromeUtils.defineESModuleGetters(this, {
    MLSuggest: "resource:///modules/urlbar/private/MLSuggest.sys.mjs",
  });

  // Override INTENT and NER options within MLSuggest
  MLSuggest.INTENT_OPTIONS = CUSTOM_INTENT_OPTIONS;
  MLSuggest.NER_OPTIONS = CUSTOM_NER_OPTIONS;
  const modelHubRootUrl = Services.env.get("MOZ_MODELS_HUB");
  if (!modelHubRootUrl) {
    throw new Error(
      "MOZ_MODELS_HUB is not set, you need to run with --hooks toolkit/components/ml/tests/tools/hook_local_hub.py"
    );
  }
  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  const { cleanup } = await perfSetup({
    prefs: [
      ["browser.ml.modelHubRootUrl", modelHubRootUrl],
      ["javascript.options.wasm_lazy_tiering", true],
    ],
  });

  await MLSuggest.initialize();
  const numIterations = 10;
  let query = "restaurants in seattle, wa";
  let names = ["intent", "ner"];
  let addColdStart = false;
  for (let name of names) {
    name = name.toUpperCase();

    let METRICS = [
      `${name}-${PIPELINE_READY_LATENCY}`,
      `${name}-${INITIALIZATION_LATENCY}`,
      `${name}-${MODEL_RUN_LATENCY}`,
      `${name}-${TOTAL_MEMORY_USAGE}`,
      ...(addColdStart
        ? [
            `${name}-${COLD_START_PREFIX}${PIPELINE_READY_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${INITIALIZATION_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${MODEL_RUN_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${TOTAL_MEMORY_USAGE}`,
          ]
        : []),
    ];

    for (let metric of METRICS) {
      journal[metric] = [];
    }
    journal["SUGGEST-model-run-latency"] = [];
  }
  for (let i = 0; i < numIterations; i++) {
    const startTime = performance.now();
    const res = await MLSuggest.makeSuggestions(query);
    const endTime = performance.now();
    const diff = Math.round(endTime - startTime);
    let intent_metrics = fetchMetrics(res.metrics.intent, false);
    let ner_metrics = fetchMetrics(res.metrics.ner, false);
    let memUsage = await getTotalMemoryUsage();
    intent_metrics[`${TOTAL_MEMORY_USAGE}`] = memUsage;
    ner_metrics[`${TOTAL_MEMORY_USAGE}`] = memUsage;

    for (let [metricName, metricVal] of Object.entries(intent_metrics)) {
      if (metricVal === null || metricVal === undefined || metricVal < 0) {
        metricVal = 0;
      }
      journal[`INTENT-${metricName}`].push(metricVal);
    }
    for (let [metricName, metricVal] of Object.entries(ner_metrics)) {
      if (metricVal === null || metricVal === undefined || metricVal < 0) {
        metricVal = 0;
      }
      journal[`NER-${metricName}`].push(metricVal);
    }
    journal[`SUGGEST-model-run-latency`].push(diff);
  }
  await MLSuggest.shutdown();
  Assert.ok(true);
  await EngineProcess.destroyMLEngine();
  await cleanup();
};

/**
 * Tests remote ML Suggest feature
 */
add_task(async function test_ml_suggest_feature() {
  await runInference2();
  reportMetrics(journal);
});
