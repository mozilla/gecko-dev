/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/// <reference path="head.js" />

requestLongerTimeout(2);

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

  Assert.equal(
    (await inferencePromise).output.echo,
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
 * Tests that we verify the task name is valid
 */
add_task(async function test_invalid_task_name() {
  const { cleanup, remoteClients } = await setup();

  const options = new PipelineOptions({ taskName: "inv#alid" });
  const mlEngineParent = await EngineProcess.getMLEngineParent();
  const engineInstance = await mlEngineParent.getEngine(options);

  let error;

  try {
    const res = engineInstance.run({ data: "This gets echoed." });
    await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);
    await res;
  } catch (e) {
    error = e;
  }
  is(
    error?.message,
    "Invalid task name. Task name should contain only alphanumeric characters and underscores/dashes.",
    "The error is correctly surfaced."
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
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
