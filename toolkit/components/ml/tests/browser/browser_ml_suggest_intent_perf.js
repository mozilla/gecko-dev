/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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
  const options = {
    taskName: "text-classification",
    modelId: "Mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
  };
  const args = ["restaurants in seattle, wa"];
  await perfTest("intent", options, args);
});
