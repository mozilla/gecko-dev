/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Suggest NER Model",
  description: "Template test for latency for ML suggest NER model",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        { name: "NER-pipeline-ready-latency", unit: "ms", shouldAlert: true },
        { name: "NER-initialization-latency", unit: "ms", shouldAlert: true },
        { name: "NER-model-run-latency", unit: "ms", shouldAlert: true },
        { name: "NER-pipeline-ready-memory", unit: "MB", shouldAlert: true },
        { name: "NER-initialization-memory", unit: "MB", shouldAlert: true },
        { name: "NER-model-run-memory", unit: "MB", shouldAlert: true },
        { name: "NER-total-memory-usage", unit: "MB", shouldAlert: true },
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
 * Tests local suggest NER model
 */
add_task(async function test_ml_ner_perftest() {
  const options = {
    taskName: "token-classification",
    modelId: "Mozilla/distilbert-uncased-NER-LoRA",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
  };

  const args = ["restaurants in seattle, wa"];
  await perfTest("ner", options, args);
});
