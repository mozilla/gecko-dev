/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Speech T5 TTS",
  description: "Testing Speech T5 TTS",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "latency",
          unit: "ms",
          shouldAlert: false,
        },
        {
          name: "memory",
          unit: "MiB",
          shouldAlert: false,
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

// Text-to-speech model tests
add_task(async function test_ml_tts() {
  const options = new PipelineOptions({
    taskName: "text-to-speech",
    modelId: "Xenova/speecht5_tts",
    modelHubUrlTemplate: "{model}/{revision}",
    modelRevision: "main",
    dtype: "q8",
    timeoutMS: 2 * 60 * 1000,
  });

  const requestInfo = {
    inputArgs: "The one ring to rule them all.",
    runOptions: {
      speaker_embeddings: `${Services.env.get("MOZ_MODELS_HUB")}/Xenova/transformers.js-docs/main/speaker_embeddings.bin`,
      vocoder: `${Services.env.get("MOZ_MODELS_HUB")}/Xenova/speecht5_hifigan`,
    },
  };

  const request = {
    args: [requestInfo.inputArgs],
    options: requestInfo.runOptions,
  };

  info(`is request null | ${request === null || request === undefined}`);

  await perfTest({
    name: "speecht5_tts",
    options,
    request,
    iterations: 10,
    addColdStart: true,
    trackPeakMemory: false,
  });
});
