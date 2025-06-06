/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

requestLongerTimeout(2);

const RAW_PIPELINE_OPTIONS = { taskName: "moz-echo", timeoutMS: -1 };

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

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

/**
 * Check that model download telemetry is working as expected
 */
add_task(async function test_model_download_telemetry_success() {
  let initialModelDownloadsCount =
    Glean.firefoxAiRuntime.modelDownload.testGetValue()?.length || 0;
  // Allow any url
  Services.env.set("MOZ_ALLOW_EXTERNAL_ML_HUB", "true");

  const originalWorkerConfig = MLEngineParent.getWorkerConfig();

  // Mocking function used in the workers or child doesn't work.
  // So we are stubbing the code run by the worker.
  const workerCode = `
  // Import the original worker code

  importScripts("${originalWorkerConfig.url}");

  // Stub
  ChromeUtils.defineESModuleGetters(
  lazy,
  {
    createFileUrl: "chrome://global/content/ml/Utils.sys.mjs",

  },
  { global: "current" }
);

  // Change the getBackend to a mocked version that doesn't actually do inference
  // but does initiate model downloads

  lazy.getBackend = async function (
    mlEngineWorker,
    _,
    {
      modelHubUrlTemplate,
      modelHubRootUrl,
      modelId,
      modelRevision,
      modelFile,
      engineId,
    } = {}
  ) {
    const url = lazy.createFileUrl({
      model: modelId,
      revision: modelRevision,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });

    const result = await mlEngineWorker.getModelFile(url).catch(() => {});

    // Download Another file using engineId as revision
    const url2 = lazy.createFileUrl({
      model: modelId,
      revision: engineId,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });
    const result2 = await mlEngineWorker.getModelFile(url2).catch(() => {});

    return {
      run: () => {},
    };
  };
`;

  const blob = new Blob([workerCode], { type: "application/javascript" });
  const blobURL = URL.createObjectURL(blob);

  let wasmBufferStub = sinon
    .stub(MLEngineParent, "getWasmArrayBuffer")
    .returns(new ArrayBuffer(16));

  let promiseStub = sinon
    .stub(MLEngineParent, "getWorkerConfig")
    .callsFake(function () {
      return { url: blobURL, options: {} };
    });

  await IndexedDBCache.init({ reset: true });
  await EngineProcess.destroyMLEngine();

  await createEngine({
    engineId: "main",
    taskName: "real-wllama-text-generation",
    featureId: "link-preview",
    backend: "wllama",
    modelId: "acme/bert",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
    modelRevision: "v0.1",
    modelHubRootUrl:
      "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data",
    modelFile: "onnx/config.json",
  });

  let observed = Glean.firefoxAiRuntime.modelDownload.testGetValue();

  Assert.equal(observed?.length || 0, initialModelDownloadsCount + 6);

  observed = observed.slice(-6);

  Assert.equal(new Set(observed.map(obj => obj.extra.modelDownloadId)).size, 1);

  Assert.deepEqual(
    observed.map(obj => obj.extra.step),
    [
      "start_download",
      "start_file_download",
      "end_file_download_success",
      "start_file_download",
      "end_file_download_success",
      "end_download_success",
    ]
  );
  await EngineProcess.destroyMLEngine();
  await IndexedDBCache.init({ reset: true });

  wasmBufferStub.restore();
  promiseStub.restore();
});

/**
 * Check that model download telemetry is working as expected
 */
add_task(async function test_model_download_telemetry_fail() {
  let initialModelDownloadsCount =
    Glean.firefoxAiRuntime.modelDownload.testGetValue()?.length || 0;
  // Allow any url
  Services.env.set("MOZ_ALLOW_EXTERNAL_ML_HUB", "true");

  const originalWorkerConfig = MLEngineParent.getWorkerConfig();

  // Mocking function used in the workers or child doesn't work.
  // So we are stubbing the code run by the worker.
  const workerCode = `
  // Import the original worker code

  importScripts("${originalWorkerConfig.url}");

  // Stub
  ChromeUtils.defineESModuleGetters(
  lazy,
  {
    createFileUrl: "chrome://global/content/ml/Utils.sys.mjs",

  },
  { global: "current" }
);

  // Change the getBackend to a mocked version that doesn't actually do inference
  // but does initiate model downloads

  lazy.getBackend = async function (
    mlEngineWorker,
    _,
    {
      modelHubUrlTemplate,
      modelHubRootUrl,
      modelId,
      modelRevision,
      modelFile,
      engineId,
    } = {}
  ) {
    const url = lazy.createFileUrl({
      model: modelId,
      revision: modelRevision,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });

    const result = await mlEngineWorker.getModelFile(url).catch(() => {});

    // Download Another file using engineId as revision
    const url2 = lazy.createFileUrl({
      model: modelId,
      revision: engineId,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });
    const result2 = await mlEngineWorker.getModelFile(url2).catch(() => {});

    return {
      run: () => {},
    };
  };
`;

  const blob = new Blob([workerCode], { type: "application/javascript" });
  const blobURL = URL.createObjectURL(blob);

  let wasmBufferStub = sinon
    .stub(MLEngineParent, "getWasmArrayBuffer")
    .returns(new ArrayBuffer(16));

  let promiseStub = sinon
    .stub(MLEngineParent, "getWorkerConfig")
    .callsFake(function () {
      return { url: blobURL, options: {} };
    });

  await IndexedDBCache.init({ reset: true });
  await EngineProcess.destroyMLEngine();
  await createEngine({
    engineId: "main",
    taskName: "real-wllama-text-generation",
    featureId: "link-preview",
    backend: "wllama",
    modelId: "acme-not-found/bert",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
    modelRevision: "v0.1",
    modelHubRootUrl:
      "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data",
    modelFile: "onnx/config.json",
  }).catch(() => {});

  let observed = Glean.firefoxAiRuntime.modelDownload.testGetValue();

  Assert.equal(observed?.length || 0, initialModelDownloadsCount + 6);

  observed = observed.slice(-6);

  Assert.equal(new Set(observed.map(obj => obj.extra.modelDownloadId)).size, 1);

  Assert.deepEqual(
    observed.map(obj => obj.extra.step),
    [
      "start_download",
      "start_file_download",
      "end_file_download_failed",
      "start_file_download",
      "end_file_download_failed",
      "end_download_failed",
    ]
  );

  await EngineProcess.destroyMLEngine();
  await IndexedDBCache.init({ reset: true });

  wasmBufferStub.restore();
  promiseStub.restore();
});

/**
 * Check that model download telemetry is working as expected
 */
add_task(async function test_model_download_telemetry_mixed() {
  let initialModelDownloadsCount =
    Glean.firefoxAiRuntime.modelDownload.testGetValue()?.length || 0;
  // Allow any url
  Services.env.set("MOZ_ALLOW_EXTERNAL_ML_HUB", "true");

  const originalWorkerConfig = MLEngineParent.getWorkerConfig();

  // Mocking function used in the workers or child doesn't work.
  // So we are stubbing the code run by the worker.
  const workerCode = `
  // Import the original worker code

  importScripts("${originalWorkerConfig.url}");

  // Stub
  ChromeUtils.defineESModuleGetters(
  lazy,
  {
    createFileUrl: "chrome://global/content/ml/Utils.sys.mjs",

  },
  { global: "current" }
);

  // Change the getBackend to a mocked version that doesn't actually do inference
  // but does initiate model downloads

  lazy.getBackend = async function (
    mlEngineWorker,
    _,
    {
      modelHubUrlTemplate,
      modelHubRootUrl,
      modelId,
      modelRevision,
      modelFile,
      engineId,
    } = {}
  ) {
    const url = lazy.createFileUrl({
      model: modelId,
      revision: modelRevision,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });

    const result = await mlEngineWorker.getModelFile(url).catch(() => {});

    // Download Another file using engineId as revision
    const url2 = lazy.createFileUrl({
      model: modelId,
      revision: engineId,
      file: modelFile,
      urlTemplate: modelHubUrlTemplate,
      rootUrl: modelHubRootUrl,
    });
    const result2 = await mlEngineWorker.getModelFile(url2).catch(() => {});

    return {
      run: () => {},
    };
  };
`;

  const blob = new Blob([workerCode], { type: "application/javascript" });
  const blobURL = URL.createObjectURL(blob);

  let wasmBufferStub = sinon
    .stub(MLEngineParent, "getWasmArrayBuffer")
    .returns(new ArrayBuffer(16));

  let promiseStub = sinon
    .stub(MLEngineParent, "getWorkerConfig")
    .callsFake(function () {
      return { url: blobURL, options: {} };
    });

  await createEngine({
    engineId: "main",
    taskName: "real-wllama-text-generation",
    featureId: "link-preview",
    backend: "wllama",
    modelId: "acme/bert",
    modelHubUrlTemplate: "{model}/resolve/{revision}",
    modelRevision: "v0.4",
    modelHubRootUrl:
      "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data",
    modelFile: "onnx/config.json",
  }).catch(() => {});

  let observed = Glean.firefoxAiRuntime.modelDownload.testGetValue();

  Assert.equal(observed?.length || 0, initialModelDownloadsCount + 6);

  observed = observed.slice(-6);

  Assert.equal(new Set(observed.map(obj => obj.extra.modelDownloadId)).size, 1);

  Assert.deepEqual(
    observed.map(obj => obj.extra.step),
    [
      "start_download",
      "start_file_download",
      "end_file_download_failed",
      "start_file_download",
      "end_file_download_success",
      "end_download_success",
    ]
  );
  await EngineProcess.destroyMLEngine();
  await IndexedDBCache.init({ reset: true });

  wasmBufferStub.restore();
  promiseStub.restore();
});
