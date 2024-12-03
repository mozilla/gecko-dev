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
        { name: "latency", unit: "ms", shouldAlert: true },
        { name: "memory", unit: "MB", shouldAlert: true },
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

  const request = {
    args,
    options: { pooling: "mean", normalize: true },
  };
  await perfTest("ner", options, request);
});
