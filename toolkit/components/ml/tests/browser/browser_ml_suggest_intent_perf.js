/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ITERATIONS = 10;

const PREFIX = "intent";
const METRICS = [
  `${PREFIX}-${PIPELINE_READY_LATENCY}`,
  `${PREFIX}-${INITIALIZATION_LATENCY}`,
  `${PREFIX}-${MODEL_RUN_LATENCY}`,
  `${PREFIX}-${PIPELINE_READY_MEMORY}`,
  `${PREFIX}-${INITIALIZATION_MEMORY}`,
  `${PREFIX}-${MODEL_RUN_MEMORY}`,
  `${PREFIX}-${TOTAL_MEMORY_USAGE}`,
];
const journal = {};
for (let metric of METRICS) {
  journal[metric] = [];
}

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Suggest Intent Model",
  description: "Template test for latency for ML suggest Intent Model",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "intent-pipeline-ready-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "intent-initialization-latency",
          unit: "ms",
          shouldAlert: true,
        },
        { name: "intent-model-run-latency", unit: "ms", shouldAlert: true },
        { name: "intent-pipeline-ready-memory", unit: "MB", shouldAlert: true },
        { name: "intent-initialization-memory", unit: "MB", shouldAlert: true },
        { name: "intent-model-run-memory", unit: "MB", shouldAlert: true },
        {
          name: "intent-total-memory-usage",
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

/**
 * Tests local suggest intent model
 */
add_task(async function test_ml_generic_pipeline() {
  const modelDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}/onnx-models`
  );
  info(`Model Directory: ${modelDirectory}`);
  const { baseUrl: modelHubRootUrl } = startHttpServer(modelDirectory);
  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  const { cleanup } = await perfSetup({
    prefs: [
      ["browser.ml.modelHubRootUrl", modelHubRootUrl],
      ["javascript.options.wasm_lazy_tiering", true],
    ],
  });

  const options = new PipelineOptions({
    taskName: "text-classification",
    modelId: "Mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
  });

  const args = ["restaurants in seattle, wa"];

  for (let i = 0; i < ITERATIONS; i++) {
    let metrics = await runInference(options, args);
    for (let [metricName, metricVal] of Object.entries(metrics)) {
      if (metricName !== `${MODEL_RUN_MEMORY}`) {
        Assert.ok(metricVal >= 0, "Metric should be non-negative.");
      }
      journal[`${PREFIX}-${metricName}`].push(metricVal);
    }
  }
  reportMetrics(journal);
  await cleanup();
});
