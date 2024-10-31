/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ITERATIONS = 10;

const METRICS = [
  PIPELINE_READY_LATENCY,
  INITIALIZATION_LATENCY,
  MODEL_RUN_LATENCY,
  PIPELINE_READY_MEMORY,
  INITIALIZATION_MEMORY,
  MODEL_RUN_MEMORY,
];
const journal = {};
for (let metric of METRICS) {
  journal[metric] = [];
}

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Test Model",
  description: "Template test for latency for ml models",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        { name: "pipeline-ready-latency", unit: "ms", shouldAlert: true },
        { name: "initialization-latency", unit: "ms", shouldAlert: true },
        { name: "model-run-latency", unit: "ms", shouldAlert: true },
        { name: "pipeline-ready-memory", unit: "MB", shouldAlert: true },
        { name: "initialization-memory", unit: "MB", shouldAlert: true },
        { name: "model-run-memory", unit: "MB", shouldAlert: true },
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
 * Tests remote ml model
 */
add_task(async function test_ml_generic_pipeline() {
  const options = new PipelineOptions({
    taskName: "feature-extraction",
    modelId: "Xenova/all-MiniLM-L6-v2",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
  });

  const args = ["The quick brown fox jumps over the lazy dog."];

  for (let i = 0; i < ITERATIONS; i++) {
    let metrics = await runInference(options, args);
    for (let [metricName, metricVal] of Object.entries(metrics)) {
      Assert.ok(metricVal >= 0, "Metric should be non-negative.");
      journal[metricName].push(metricVal);
    }
  }
  reportMetrics(journal);
});
