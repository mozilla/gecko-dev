/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RAW_PIPELINE_OPTIONS = { taskName: "moz-echo", timeoutMS: -1 };

function getGleanCount(metricsName, engineId = "default-engine") {
  var metrics = Glean.firefoxAiRuntime[metricsName];

  // events
  if (["runInferenceFailure", "engineCreationFailure"].includes(metricsName)) {
    return metrics.testGetValue()?.length || 0;
  }

  // labeled timing distribution
  return metrics[engineId]?.testGetValue()?.count || 0;
}

/**
 * Check that we record the engine creation and the inference run
 */
add_task(async function test_default_telemetry() {
  const { cleanup, remoteClients } = await setup();
  const engineCreationSuccessCount = getGleanCount("engineCreationSuccess");
  const runInferenceSuccessCount = getGleanCount("runInferenceSuccess");
  const runInferenceFailureCount = getGleanCount("runInferenceFailure");
  const engineCreationFailureCount = getGleanCount("engineCreationFailure");

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

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

  Assert.equal(
    getGleanCount("engineCreationSuccess"),
    engineCreationSuccessCount + 1
  );

  Assert.equal(
    getGleanCount("engineCreationSuccess"),
    engineCreationSuccessCount + 1
  );

  Assert.equal(
    getGleanCount("runInferenceSuccess"),
    runInferenceSuccessCount + 1
  );

  Assert.equal(getGleanCount("runInferenceFailure"), runInferenceFailureCount);

  Assert.equal(
    getGleanCount("engineCreationFailure"),
    engineCreationFailureCount
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Check that we record the engine creation and the inference failure
 */
add_task(async function test_ml_engine_run_failure() {
  const { cleanup, remoteClients } = await setup();
  const engineCreationSuccessCount = getGleanCount("engineCreationSuccess");
  const runInferenceSuccessCount = getGleanCount("runInferenceSuccess");
  const runInferenceFailureCount = getGleanCount("runInferenceFailure");
  const engineCreationFailureCount = getGleanCount("engineCreationFailure");

  info("Get the engine");
  const engineInstance = await createEngine(RAW_PIPELINE_OPTIONS);

  info("Run the inference with a throwing example.");
  const inferencePromise = engineInstance.run("throw");

  info("Wait for the pending downloads.");
  await remoteClients["ml-onnx-runtime"].resolvePendingDownloads(1);

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

  Assert.equal(
    getGleanCount("engineCreationSuccess"),
    engineCreationSuccessCount + 1
  );

  Assert.equal(getGleanCount("runInferenceSuccess"), runInferenceSuccessCount);

  Assert.equal(
    getGleanCount("runInferenceFailure"),
    runInferenceFailureCount + 1
  );

  Assert.equal(
    getGleanCount("engineCreationFailure"),
    engineCreationFailureCount
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});

/**
 * Check that we record the engine creation failure
 */
add_task(async function test_engine_creation_failure() {
  const { cleanup } = await setup();
  const engineCreationSuccessCount = getGleanCount("engineCreationSuccess");
  const engineCreationFailureCount = getGleanCount("engineCreationFailure");
  const runInferenceSuccessCount = getGleanCount("runInferenceSuccess");
  const runInferenceFailureCount = getGleanCount("runInferenceFailure");

  try {
    await createEngine({ taskName: "moz-echo", featureId: "I DONT EXIST" });
  } catch (e) {}

  Assert.equal(
    getGleanCount("engineCreationSuccess"),
    engineCreationSuccessCount
  );

  Assert.equal(
    getGleanCount("engineCreationSuccess"),
    engineCreationSuccessCount
  );

  Assert.equal(getGleanCount("runInferenceSuccess"), runInferenceSuccessCount);

  Assert.equal(getGleanCount("runInferenceFailure"), runInferenceFailureCount);

  Assert.equal(
    getGleanCount("engineCreationFailure"),
    engineCreationFailureCount + 1
  );

  await EngineProcess.destroyMLEngine();
  await cleanup();
});
