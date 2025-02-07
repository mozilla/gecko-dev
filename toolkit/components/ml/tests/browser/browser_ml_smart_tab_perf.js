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
