/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Test Model",
  description: "Template test for latency for ml models",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "EXAMPLE-cold-start-pipeline-ready-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-initialization-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-model-run-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-pipeline-ready-memory",
          unit: "MB",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-initialization-memory",
          unit: "MB",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-model-run-memory",
          unit: "MB",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-cold-start-total-memory-usage",
          unit: "MB",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-pipeline-ready-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-initialization-latency",
          unit: "ms",
          shouldAlert: true,
        },
        { name: "EXAMPLE-model-run-latency", unit: "ms", shouldAlert: true },
        {
          name: "EXAMPLE-pipeline-ready-memory",
          unit: "MB",
          shouldAlert: true,
        },
        {
          name: "EXAMPLE-initialization-memory",
          unit: "MB",
          shouldAlert: true,
        },
        { name: "EXAMPLE-model-run-memory", unit: "MB", shouldAlert: true },
        { name: "EXAMPLE-total-memory-usage", unit: "MB", shouldAlert: true },
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
  const options = {
    taskName: "feature-extraction",
    modelId: "Xenova/all-MiniLM-L6-v2",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
  };

  const args = ["The quick brown fox jumps over the lazy dog."];
  await perfTest("example", options, args, ITERATIONS, true);
});
