/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/// <reference path="head.js" />

requestLongerTimeout(2);

const RAW_PIPELINE_OPTIONS = { taskName: "moz-echo" };
const PIPELINE_OPTIONS = new PipelineOptions({ taskName: "moz-echo" });

async function checkForRemoteType(remoteType) {
  let procinfo3 = await ChromeUtils.requestProcInfo();
  for (const child of procinfo3.children) {
    if (child.type === remoteType) {
      return true;
    }
  }
  return false;
}

add_task(async function test_ml_engine_basics() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  const res = await inferencePromise;
  Assert.equal(
    res.output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.equal(res.output.dtype, "q8", "The config was enriched by RS");
  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

add_task(async function test_ml_engine_pick_feature_id() {
  // one record sent back from RS contains featureId
  const records = [
    {
      taskName: "moz-echo",
      modelId: "mozilla/distilvit",
      processorId: "mozilla/distilvit",
      tokenizerId: "mozilla/distilvit",
      modelRevision: "main",
      processorRevision: "main",
      tokenizerRevision: "main",
      dtype: "q8",
      id: "74a71cfd-1734-44e6-85c0-69cf3e874138",
    },
    {
      featureId: "pdfjs-alt-text",
      taskName: "moz-echo",
      modelId: "mozilla/distilvit",
      processorId: "mozilla/distilvit",
      tokenizerId: "mozilla/distilvit",
      modelRevision: "v1.0",
      processorRevision: "v1.0",
      tokenizerRevision: "v1.0",
      dtype: "fp16",
      id: "74a71cfd-1734-44e6-85c0-69cf3e874138",
    },
  ];

  const { cleanup, remoteClients } = await setup({ records });

  info("Get the engine");
  const engineInstance = await createEngine({
    featureId: "pdfjs-alt-text",
    taskName: "moz-echo",
  });

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  const res = await inferencePromise;
  Assert.equal(
    res.output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.equal(
    res.output.dtype,
    "fp16",
    "The config was enriched by RS - using a feature Id"
  );
  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

add_task(async function test_ml_engine_wasm_rejection() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].rejectPendingDownloads(1);
  //await remoteClients.models.resolvePendingDownloads(1);

  let error;
  try {
    await inferencePromise;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    "Intentionally rejecting downloads.",
    "The error is correctly surfaced."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests that the engineInstanceModel's internal errors are correctly surfaced.
 */
add_task(async function test_ml_engine_model_error() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

  info("Run the inference with a throwing example.");
  const inferencePromise = engineInstance.run("throw");

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
  //await remoteClients.models.resolvePendingDownloads(1);

  let error;
  try {
    await inferencePromise;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    'Error: Received the message "throw", so intentionally throwing an error.',
    "The error is correctly surfaced."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * This test is really similar to the "basic" test, but tests manually destroying
 * the engineInstance.
 */
add_task(async function test_ml_engine_destruction() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");
  const engineInstance = await mlEngineParent.getEngine(PIPELINE_OPTIONS);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await engineInstance.terminate(
    /* shutDownIfEmpty */ true,
    /* replacement */ false
  );

  info(
    "The engineInstance is manually destroyed. The cleanup function should wait for the engine process to be destroyed."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests that we display a nice error message when the pref is off
 */
add_task(async function test_pref_is_off() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.enable", false]],
  });

  info("Get the engine process");
  let error;

  try {
    await EngineProcess.getMLEngineParent();
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    "MLEngine is disabled. Check the browser.ml prefs.",
    "The error is correctly surfaced."
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.ml.enable", true]],
  });
});

/**
 * Tests the generic pipeline API
 */
add_task(async function test_ml_generic_pipeline() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");

  const options = new PipelineOptions({
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  });

  const engineInstance = await mlEngineParent.getEngine(options);

  info("Run the inference");
  const inferencePromise = engineInstance.run({
    args: ["This gets echoed."],
  });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests that the engine is reused.
 */
add_task(async function test_ml_engine_reuse_same() {
  const { cleanup, remoteClients } = await setup();

  const options = { taskName: "moz-echo", engineId: "echo" };
  const engineInstance = await createEngine(options);
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  let engineInstance2 = await createEngine(options);
  is(engineInstance2.engineId, "echo", "The engine ID matches");
  is(engineInstance, engineInstance2, "The engine is reused.");
  const inferencePromise2 = engineInstance2.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise2).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests that we can have two competing engines
 */
add_task(async function test_ml_two_engines() {
  const { cleanup, remoteClients } = await setup();

  const engineInstance = await createEngine({
    taskName: "moz-echo",
    engineId: "engine1",
  });
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  let engineInstance2 = await createEngine({
    taskName: "moz-echo",
    engineId: "engine2",
  });

  const inferencePromise2 = engineInstance2.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise2).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.notEqual(
    engineInstance.engineId,
    engineInstance2.engineId,
    "Should be different engines"
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests that we can have the same engine reinitialized
 */
add_task(async function test_ml_dupe_engines() {
  const { cleanup, remoteClients } = await setup();

  const engineInstance = await createEngine({
    taskName: "moz-echo",
    engineId: "engine1",
  });
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  let engineInstance2 = await createEngine({
    taskName: "moz-echo",
    engineId: "engine1",
    timeoutMS: 2000, // that makes the options different
  });
  const inferencePromise2 = engineInstance2.run({ data: "This gets echoed." });
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise2).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.notEqual(
    engineInstance,
    engineInstance2,
    "Should be different engines"
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

add_task(async function test_ml_engine_override_options() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine({
    taskName: "moz-echo",
    modelRevision: "v1",
  });

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  Assert.equal(
    (await inferencePromise).output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.equal(
    (await inferencePromise).output.modelRevision,
    "v1",
    "The config options goes through and overrides."
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Tests a custom model hub
 */
add_task(async function test_ml_custom_hub() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");

  const options = new PipelineOptions({
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
    modelHubRootUrl: "https://example.com",
    modelHubUrlTemplate: "models/{model}/{revision}",
  });

  const engineInstance = await mlEngineParent.getEngine(options);

  info("Run the inference");
  const inferencePromise = engineInstance.run({
    args: ["This gets echoed."],
  });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  let res = await inferencePromise;

  Assert.equal(
    res.output,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.equal(
    res.config.modelHubRootUrl,
    "https://example.com",
    "The pipeline used the custom hub"
  );

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Make sure we don't get race conditions when running several inference runs in parallel
 *
 */

add_task(async function test_ml_engine_parallel() {
  const { cleanup, remoteClients } = await setup();

  // We're doing 10 calls and each echo call will take from 0 to 1000ms
  // So we're sure we're mixing runs.
  let sleepTimes = [300, 1000, 700, 0, 500, 900, 400, 800, 600, 100];
  let numCalls = 10;

  async function run(x) {
    const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

    let msg = `${x} - This gets echoed.`;
    let res = engineInstance.run({
      data: msg,
      sleepTime: sleepTimes[x],
    });

    await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
    res = await res;

    return res;
  }

  info(`Run ${numCalls} inferences in parallel`);
  let runs = [];
  for (let x = 0; x < numCalls; x++) {
    runs.push(run(x));
  }

  // await all runs
  const results = await Promise.all(runs);
  Assert.equal(results.length, numCalls, `All ${numCalls} were successful`);

  // check that each one got their own stuff
  for (let y = 0; y < numCalls; y++) {
    Assert.equal(
      results[y].output.echo,
      `${y} - This gets echoed.`,
      `Result ${y} is correct`
    );
  }

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

/**
 * Test threading support
 */
add_task(async function test_ml_threading_support() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get engineInstance");

  const options = new PipelineOptions({
    taskName: "summarization",
    modelId: "test-echo",
    modelRevision: "main",
  });

  const engineInstance = await mlEngineParent.getEngine(options);

  info("Run the inference");
  const inferencePromise = engineInstance.run({
    args: ["This gets echoed."],
  });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  let res = await inferencePromise;

  ok(res.multiThreadSupported, "Multi-thread should be supported");
  await EngineProcess.destroyMLEngine();
  await cleanup();
});

add_task(async function test_ml_engine_get_status() {
  const { cleanup, remoteClients } = await setup();

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  const res = await inferencePromise;
  Assert.equal(
    res.output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  const expected = {
    "default-engine": {
      status: "IDLING",
      options: {
        engineId: "default-engine",
        featureId: null,
        taskName: "moz-echo",
        timeoutMS: 1000,
        modelHubRootUrl:
          "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data",
        modelHubUrlTemplate: "{model}/{revision}",
        modelId: "mozilla/distilvit",
        modelRevision: "main",
        tokenizerId: "mozilla/distilvit",
        tokenizerRevision: "main",
        processorId: "mozilla/distilvit",
        processorRevision: "main",
        logLevel: "All",
        runtimeFilename: "ort-wasm-simd-threaded.jsep.wasm",
        device: null,
        dtype: "q8",
        numThreads: null,
        executionPriority: null,
        modelHub: null,
      },
      engineId: "default-engine",
    },
  };

  let status = await engineInstance.mlEngineParent.getStatus();
  status = JSON.parse(JSON.stringify(Object.fromEntries(status)));
  Assert.deepEqual(status, expected);

  await ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

add_task(async function test_ml_engine_not_enough_memory() {
  const { cleanup, remoteClients } = await setup({
    prefs: [["browser.ml.checkForMemory", true]],
  });

  info("Get the greedy engine");
  const engineInstance = await createEngine({
    modelId: "testing/greedy",
    taskName: "summarization",
    dtype: "q8",
    numThreads: 1,
    device: "wasm",
  });
  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  await Assert.rejects(
    inferencePromise,
    /Timeout reached while waiting for enough memory/,
    "The call should be rejected because of a lack of memory"
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Helper function to create a basic set of valid options
 */
function getValidOptions(overrides = {}) {
  return Object.assign(
    {
      engineId: "validEngine1",
      featureId: "pdfjs-alt-text",
      taskName: "valid_task",
      modelHubRootUrl: "https://example.com",
      modelHubUrlTemplate: "https://example.com/{modelId}",
      timeoutMS: 5000,
      modelId: "validModel",
      modelRevision: "v1",
      tokenizerId: "validTokenizer",
      tokenizerRevision: "v1",
      processorId: "validProcessor",
      processorRevision: "v1",
      logLevel: null,
      runtimeFilename: "runtime.wasm",
      device: InferenceDevice.GPU,
      numThreads: 4,
      executionPriority: ExecutionPriority.NORMAL,
    },
    overrides
  );
}

/**
 * A collection of test cases for invalid and valid values.
 */
const commonInvalidCases = [
  { description: "Invalid value (special characters)", value: "org1/my!value" },
  {
    description: "Invalid value (special characters in organization)",
    value: "org@1/my-value",
  },
  { description: "Invalid value (missing name part)", value: "org1/" },
  {
    description: "Invalid value (invalid characters in name)",
    value: "my$value",
  },
];

const commonValidCases = [
  { description: "Valid organization/name", value: "org1/my-value" },
  { description: "Valid name only", value: "my-value" },
  {
    description: "Valid name with underscores and dashes",
    value: "my_value-123",
  },
  {
    description: "Valid organization with underscores and dashes",
    value: "org_123/my-value",
  },
];

const pipelineOptionsCases = [
  // Invalid cases for various fields
  ...commonInvalidCases.map(test => ({
    description: `Invalid processorId (${test.description})`,
    options: { processorId: test.value },
    expectedError: /Invalid value/,
  })),
  ...commonInvalidCases.map(test => ({
    description: `Invalid tokenizerId (${test.description})`,
    options: { tokenizerId: test.value },
    expectedError: /Invalid value/,
  })),
  ...commonInvalidCases.map(test => ({
    description: `Invalid modelId (${test.description})`,
    options: { modelId: test.value },
    expectedError: /Invalid value/,
  })),

  // Valid cases for various fields
  ...commonValidCases.map(test => ({
    description: `Valid processorId (${test.description})`,
    options: { processorId: test.value },
    expected: { processorId: test.value },
  })),
  ...commonValidCases.map(test => ({
    description: `Valid tokenizerId (${test.description})`,
    options: { tokenizerId: test.value },
    expected: { tokenizerId: test.value },
  })),
  ...commonValidCases.map(test => ({
    description: `Valid modelId (${test.description})`,
    options: { modelId: test.value },
    expected: { modelId: test.value },
  })),

  // Invalid values
  {
    description: "Invalid hub",
    options: { modelHub: "rogue" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid timeoutMS",
    options: { timeoutMS: -3 },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid timeoutMS",
    options: { timeoutMS: 40000000 },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid featureId",
    options: { featureId: "unknown" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid dtype",
    options: { dtype: "invalid_dtype" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid device",
    options: { device: "invalid_device" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid executionPriority",
    options: { executionPriority: "invalid_priority" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid logLevel",
    options: { logLevel: "invalid_log_level" },
    expectedError: /Invalid value/,
  },

  // Valid values
  {
    description: "valid hub",
    options: { modelHub: "huggingface" },
    expected: { modelHub: "huggingface" },
  },
  {
    description: "valid hub",
    options: { modelHub: "mozilla" },
    expected: { modelHub: "mozilla" },
  },
  {
    description: "valid timeoutMS",
    options: { timeoutMS: 12345 },
    expected: { timeoutMS: 12345 },
  },
  {
    description: "valid timeoutMS",
    options: { timeoutMS: -1 },
    expected: { timeoutMS: -1 },
  },

  {
    description: "Valid dtype",
    options: { dtype: QuantizationLevel.FP16 },
    expected: { dtype: QuantizationLevel.FP16 },
  },
  {
    description: "Valid device",
    options: { device: InferenceDevice.WASM },
    expected: { device: InferenceDevice.WASM },
  },
  {
    description: "Valid executionPriority",
    options: { executionPriority: ExecutionPriority.HIGH },
    expected: { executionPriority: ExecutionPriority.HIGH },
  },
  {
    description: "Valid logLevel (Info)",
    options: { logLevel: LogLevel.INFO },
    expected: { logLevel: LogLevel.INFO },
  },
  {
    description: "Valid logLevel (Critical)",
    options: { logLevel: LogLevel.CRITICAL },
    expected: { logLevel: LogLevel.CRITICAL },
  },
  {
    description: "Valid logLevel (All)",
    options: { logLevel: LogLevel.ALL },
    expected: { logLevel: LogLevel.ALL },
  },
  {
    description: "Valid modelId",
    options: { modelId: "Qwen2.5-0.5B-Instruct" },
    expected: { modelId: "Qwen2.5-0.5B-Instruct" },
  },

  // Invalid revision cases
  {
    description: "Invalid revision (random string)",
    options: { modelRevision: "invalid_revision" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid revision (too many version numbers)",
    options: { tokenizerRevision: "v1.0.3.4.5" },
    expectedError: /Invalid value/,
  },
  {
    description: "Invalid revision (unknown suffix)",
    options: { processorRevision: "v1.0.0-unknown" },
    expectedError: /Invalid value/,
  },

  // Valid revision cases with new format
  {
    description: "Valid revision (main)",
    options: { modelRevision: "main" },
    expected: { modelRevision: "main" },
  },
  {
    description: "Valid revision (v-prefixed version with alpha)",
    options: { tokenizerRevision: "v1.2.3-alpha1" },
    expected: { tokenizerRevision: "v1.2.3-alpha1" },
  },
  {
    description:
      "Valid revision (v-prefixed version with beta and dot separator)",
    options: { tokenizerRevision: "v1.2.3.beta2" },
    expected: { tokenizerRevision: "v1.2.3.beta2" },
  },
  {
    description:
      "Valid revision (non-prefixed version with rc and dash separator)",
    options: { processorRevision: "1.0.0-rc3" },
    expected: { processorRevision: "1.0.0-rc3" },
  },
  {
    description:
      "Valid revision (non-prefixed version with pre and dot separator)",
    options: { processorRevision: "1.0.0.pre4" },
    expected: { processorRevision: "1.0.0.pre4" },
  },
  {
    description: "Valid revision (version without suffix)",
    options: { modelRevision: "1.0.0" },
    expected: { modelRevision: "1.0.0" },
  },

  // Valid engineID cases
  {
    description: "Valid engineID (qwen)",
    options: { engineId: "SUM-ONNX-COMMUNITY_QWEN2_5-0_5B-INSTRUCT_BIG" },
    expected: { engineId: "SUM-ONNX-COMMUNITY_QWEN2_5-0_5B-INSTRUCT_BIG" },
  },
];

/**
 * Testing PipelineOption validation
 */
add_task(async function test_pipeline_options_validation() {
  pipelineOptionsCases.forEach(testCase => {
    if (testCase.expectedError) {
      Assert.throws(
        () => new PipelineOptions(getValidOptions(testCase.options)),
        testCase.expectedError,
        `${testCase.description} throws the expected error`
      );
    } else {
      const pipelineOptions = new PipelineOptions(
        getValidOptions(testCase.options)
      );
      Object.keys(testCase.expected).forEach(key => {
        is(
          pipelineOptions[key],
          testCase.expected[key],
          `${testCase.description} sets ${key} correctly`
        );
      });
    }
  });
});

add_task(async function test_ml_engine_infinite_worker() {
  const { cleanup, remoteClients } = await setup();

  const options = { taskName: "moz-echo", timeoutMS: -1 };
  const engineInstance = await createEngine(options);

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  const res = await inferencePromise;
  Assert.equal(
    res.output.echo,
    "This gets echoed.",
    "The text get echoed exercising the whole flow."
  );

  Assert.equal(res.output.timeoutMS, -1, "This should be an infinite worker.");
  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});

add_task(async function test_ml_engine_model_hub_applied() {
  const options = {
    taskName: "moz-echo",
    timeoutMS: -1,
    modelHub: "huggingface",
  };
  const parsedOptions = new PipelineOptions(options);

  Assert.equal(
    parsedOptions.modelHubRootUrl,
    "https://huggingface.co/",
    "modelHubRootUrl is set"
  );

  Assert.equal(
    parsedOptions.modelHubUrlTemplate,
    "{model}/resolve/{revision}",
    "modelHubUrlTemplate is set"
  );
});

add_task(async function test_ml_engine_blessed_model() {
  const { cleanup, remoteClients } = await setup();

  const options = { taskName: "test-echo" };
  const engineInstance = await createEngine(options);

  info("Check the inference process is running");
  Assert.equal(await checkForRemoteType("inference"), true);

  info("Run the inference");
  const inferencePromise = engineInstance.run({ data: "This gets echoed." });

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

  const res = await inferencePromise;

  Assert.equal(
    res.config.modelId,
    "test-echo",
    "The blessed model was picked."
  );

  Assert.equal(res.config.dtype, "q8", "With the right quantization level");

  ok(
    !EngineProcess.areAllEnginesTerminated(),
    "The engine process is still active."
  );

  await EngineProcess.destroyMLEngine();

  await cleanup();
});
