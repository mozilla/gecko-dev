/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(120);

function normalizePathForOS(path) {
  if (Services.appinfo.OS === "WINNT") {
    // On Windows, replace forward slashes with backslashes
    return path.replace(/\//g, "\\");
  }

  // On Unix-like systems, replace backslashes with forward slashes
  return path.replace(/\\/g, "/");
}

async function setup({ disabled = false, prefs = [] } = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "All"],
      ["browser.ml.modelCacheTimeout", 1000],
      ...prefs,
    ],
  });

  const artifact_directory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}`
  );

  Assert.ok(
    await IOUtils.exists(artifact_directory),
    "The artifact directory does not exist. This usually happens when running locally. " +
      "Please download all the files from taskcluster/kinds/fetch/onnxruntime-web-fetch.yml. " +
      "Place them in a directory and rerun the test with the environment variable 'MOZ_FETCHES_DIR' " +
      "set such that all the files are directly inside 'MOZ_FETCHES_DIR'"
  );

  async function download(record) {
    return {
      buffer: (
        await IOUtils.read(
          normalizePathForOS(`${artifact_directory}/${record.name}`)
        )
      ).buffer,
    };
  }

  remoteClients["ml-onnx-runtime"].client.attachments.download = download;

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

function createRandomImageData(size) {
  return Uint8ClampedArray.from({ length: size }, () => Math.random() * 255);
}

function isNumberType(value) {
  return typeof value === "number" || value instanceof Number;
}

function isStringType(value) {
  return typeof value === "string" || value instanceof String;
}

/**
 * Tests moz-image-to-text pipeline API
 */
add_task(async function test_ml_moz_image_to_text_pipeline() {
  const { cleanup } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");

  const options = new PipelineOptions({
    taskName: "moz-image-to-text",
    modelId: "acme/test-vit-gpt2-image-captioning",
    processorId: "acme/test-vit-gpt2-image-captioning",
    tokenizerId: "acme/test-vit-gpt2-image-captioning",
    modelRevision: "main",
    processorRevision: "main",
    tokenizerRevision: "main",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
    dtype: "q8",
  });

  const engineInstance = await mlEngineParent.getEngine(options);

  info("Run the inference");

  const request = {
    data: createRandomImageData(224 * 224 * 3),
    channels: 3,
    height: 224,
    width: 224,
  };

  const res = await engineInstance.run(request);

  Assert.ok(
    typeof res.output === "string" || res.output instanceof String,
    "The correct type is not returned for the output"
  );

  Assert.ok(res.metrics, "Metrics is not defined");

  Assert.ok(
    isNumberType(res.metrics?.inferenceTime),
    "The correct type is not returned for inferenceTime"
  );

  Assert.ok(
    isNumberType(res.metrics?.initTime),
    "The correct type is not returned for initTime"
  );

  Assert.ok(
    isNumberType(res.metrics?.tokenizingTime),
    "The correct type is not returned for tokenizingTime"
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests generic pipeline API
 */
add_task(async function test_ml_generic_pipeline() {
  const { cleanup } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");

  const options = new PipelineOptions({
    taskName: "image-to-text",
    modelId: "acme/test-vit-gpt2-image-captioning",
    processorId: "acme/test-vit-gpt2-image-captioning",
    tokenizerId: "acme/test-vit-gpt2-image-captioning",
    modelRevision: "main",
    processorRevision: "main",
    tokenizerRevision: "main",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
    dtype: "q8",
  });

  const engineInstance = await mlEngineParent.getEngine(options);

  info("Run the inference");
  const imageUri =
    "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data/images/1.png";

  const min_new_tokens = 10;
  const max_new_tokens = 20;

  const imageResponse = await fetch(imageUri);
  const imageArrayBuffer = await imageResponse.arrayBuffer();
  const blobUrl = URL.createObjectURL(
    new Blob([imageArrayBuffer], { type: "image/png" })
  );

  // Ensure url is released regardless of uncaught exceptions.
  registerCleanupFunction(() => {
    URL.revokeObjectURL(blobUrl);
  });

  const request = {
    args: [blobUrl],
    options: { min_new_tokens, max_new_tokens },
  };

  const res = await engineInstance.run(request);

  Assert.equal(res.length, 1, "We should get exactly 1 output result");

  Assert.ok(
    isStringType(res[0].generated_text),
    "generated_text should be a string"
  );

  // "acme/test-vit-gpt2-image-captioning" tokenizer assigns each char to a token except 1.
  Assert.ok(
    res[0].generated_text.length >= min_new_tokens - 1,
    "Number of generated tokens is larger than expected"
  );

  // "acme/test-vit-gpt2-image-captioning" tokenizer assigns each char to a token except 1.
  Assert.ok(
    res[0].generated_text.length <= max_new_tokens + 1,
    "Number of generated tokens is lower than expected"
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});
