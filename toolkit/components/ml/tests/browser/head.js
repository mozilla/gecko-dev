/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/// <reference path="../../../../../toolkit/components/translations/tests/browser/shared-head.js" />

"use strict";

/**
 * @type {import("../../actors/MLEngineParent.sys.mjs")}
 */
const { MLEngineParent } = ChromeUtils.importESModule(
  "resource://gre/actors/MLEngineParent.sys.mjs"
);

const { ModelHub, IndexedDBCache } = ChromeUtils.importESModule(
  "chrome://global/content/ml/ModelHub.sys.mjs"
);

const {
  createEngine,
  PipelineOptions,
  QuantizationLevel,
  ExecutionPriority,
  InferenceDevice,
  LogLevel,
} = ChromeUtils.importESModule(
  "chrome://global/content/ml/EngineProcess.sys.mjs"
);

// This test suite shares some utility functions with translations as they work in a very
// similar fashion. Eventually, the plan is to unify these two components.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/shared-head.js",
  this
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

/**
 * Sets up the stage for a test
 *
 */
async function setup({ disabled = false, prefs = [], records = null } = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
    records,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "All"],
      ["browser.ml.modelCacheTimeout", 1000],
      ["browser.ml.checkForMemory", false],
      ["browser.ml.queueWaitTimeout", 2],
      ["javascript.options.wasm_lazy_tiering", true],
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
      await SpecialPowers.popPrefEnv();
    },
  };
}

function getDefaultWasmRecords() {
  return [
    {
      name: MLEngineParent.WASM_FILENAME,
      version: MLEngineParent.WASM_MAJOR_VERSION + ".0",
    },
  ];
}

async function createAndMockMLRemoteSettings({
  autoDownloadFromRemoteSettings = false,
  records = null,
} = {}) {
  const wasmRecords = getDefaultWasmRecords().map(({ name, version }) => ({
    id: crypto.randomUUID(),
    name,
    version,
    last_modified: Date.now(),
    schema: Date.now(),
  }));
  const runtime = await createRemoteClient({
    collectionName: "test-translation-wasm",
    records: wasmRecords,
    attachmentMock: true,
    autoDownloadFromRemoteSettings,
  });

  const options = await createRemoteClient({
    records: records || [
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
    ],
    collectionName: "test-ml-inference-options",
  });

  const allowDeny = await createRemoteClient({
    records: [
      {
        filter: "ALLOW",
        urlPrefix: "https://",
        id: "74a71cfd-1734-44e6-85c0-69cf3e874138",
      },
    ],
    collectionName: "test-ml-allow-deny-list",
  });

  const remoteClients = {
    "ml-onnx-runtime": runtime,
    "ml-inference-options": options,
    "ml-model-allow-deny-list": allowDeny,
  };

  MLEngineParent.mockRemoteSettings({
    "ml-onnx-runtime": runtime.client,
    "ml-inference-options": options,
    "ml-model-allow-deny-list": allowDeny,
  });

  return {
    async removeMocks() {
      await runtime.client.attachments.deleteAll();
      await runtime.client.db.clear();
      await options.db.clear();
      await allowDeny.db.clear();
      MLEngineParent.removeMocks();
    },
    remoteClients,
  };
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @returns {RemoteSettings|AttachmentMock}
 */
async function createRemoteClient({
  records,
  collectionName,
  attachmentMock = false,
  autoDownloadFromRemoteSettings = false,
}) {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const client = RemoteSettings(`${collectionName}-${_remoteSettingsMockId++}`);
  await client.db.clear();
  await client.db.importChanges({}, Date.now(), records);

  if (attachmentMock) {
    return createAttachmentMock(
      client,
      collectionName,
      autoDownloadFromRemoteSettings
    );
  }
  return client;
}

/*
 * Perftest related
 */
const MB_TO_BYTES = 1024 * 1024;
const INIT_START = "initializationStart";
const INIT_END = "initializationEnd";
const RUN_START = "runStart";
const RUN_END = "runEnd";
const PIPELINE_READY_START = "ensurePipelineIsReadyStart";
const PIPELINE_READY_END = "ensurePipelineIsReadyEnd";
const PIPELINE_READY_LATENCY = "pipeline-ready-latency";
const INITIALIZATION_LATENCY = "initialization-latency";
const MODEL_RUN_LATENCY = "model-run-latency";
const TOTAL_MEMORY_USAGE = "total-memory-usage";
const COLD_START_PREFIX = "cold-start-";
const ITERATIONS = 10;
const WHEN = "when";
const MEMORY = "memory";

const formatNumber = new Intl.NumberFormat("en-US", {
  maximumSignificantDigits: 4,
}).format;

function median(arr) {
  arr = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(arr.length / 2);

  if (arr.length % 2) {
    return arr[mid];
  }

  return (arr[mid - 1] + arr[mid]) / 2;
}

function stringify(arr) {
  function pad(str) {
    str = str.padStart(7, " ");
    if (str[0] != " ") {
      str = " " + str;
    }
    return str;
  }

  return arr.reduce((acc, elem) => acc + pad(formatNumber(elem)), "");
}

function reportMetrics(journal) {
  let metrics = {};
  let text = "\nResults (ms)\n";

  const names = Object.keys(journal);
  const prefixLen = 1 + Math.max(...names.map(str => str.length));

  for (const name in journal) {
    const med = median(journal[name]);
    text += (name + ":").padEnd(prefixLen, " ") + stringify(journal[name]);
    text += "   median " + formatNumber(med) + "\n";
    metrics[name] = med;
  }

  dump(text);
  info(`perfMetrics | ${JSON.stringify(metrics)}`);
}

/**
 * Fetches the latest metric entry with the specified name and retrieves its value for the given key.
 * If multiple metrics share the same name, the function returns the key from the most recent one.
 *
 * @param {Array<object>} metrics - The array of metric objects to search through.
 * @param {string} name - The name of the metric to find.
 * @param {string} key - The key within the metric object whose value should be returned.
 * @returns {*} - The value of the specified key in the latest metric with the given name, or undefined if no matching metric is found.
 */
function fetchMLMetric(metrics, name, key) {
  const matchingMetrics = metrics.filter(metric => metric.name === name);
  if (matchingMetrics.length === 0) {
    return undefined;
  } // Return undefined if no match found
  const latestMetric = matchingMetrics[matchingMetrics.length - 1];
  return latestMetric[key];
}

function fetchLatencyMetrics(metrics, isFirstRun) {
  const pipelineLatency =
    fetchMLMetric(metrics, PIPELINE_READY_END, WHEN) -
    fetchMLMetric(metrics, PIPELINE_READY_START, WHEN);
  const initLatency =
    fetchMLMetric(metrics, INIT_END, WHEN) -
    fetchMLMetric(metrics, INIT_START, WHEN);
  const runLatency =
    fetchMLMetric(metrics, RUN_END, WHEN) -
    fetchMLMetric(metrics, RUN_START, WHEN);
  return {
    [`${isFirstRun ? COLD_START_PREFIX : ""}${PIPELINE_READY_LATENCY}`]:
      pipelineLatency,
    [`${isFirstRun ? COLD_START_PREFIX : ""}${INITIALIZATION_LATENCY}`]:
      initLatency,
    [`${isFirstRun ? COLD_START_PREFIX : ""}${MODEL_RUN_LATENCY}`]: runLatency,
  };
}

function fetchMetrics(metrics, isFirstRun) {
  return {
    ...fetchLatencyMetrics(metrics, isFirstRun),
  };
}

function startHttpServer(directoryPath) {
  // Create a new HTTP server
  const server = new HttpServer();

  // Set the base directory that the server will serve files from
  const baseDirectory = new FileUtils.File(directoryPath);

  // Register a path to serve files from the directory
  server.registerDirectory("/", baseDirectory);

  // Start the server on a random available port (-1)
  server.start(-1);

  // Ensure that the server is stopped regardless of uncaught exceptions.
  registerCleanupFunction(async () => {
    // Stop the server manually before moving to the next stage
    await new Promise(resolve => server.stop(resolve));
  });

  // Get the primary port that the server is using
  const port = server.identity.primaryPort;
  const baseUrl = `http://localhost:${port}/`;

  // Return the server instance and the base URL
  return { server, baseUrl };
}

async function initializeEngine(pipelineOptions) {
  const modelDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}/onnx-models`
  );
  info(`Model Directory: ${modelDirectory}`);
  const { baseUrl: modelHubRootUrl } = startHttpServer(modelDirectory);
  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  const { cleanup } = await perfSetup({
    prefs: [["browser.ml.modelHubRootUrl", modelHubRootUrl]],
  });
  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get Pipeline Options");
  info("Run the inference");
  return {
    cleanup,
    engine: await mlEngineParent.getEngine(pipelineOptions),
  };
}

function normalizePathForOS(path) {
  if (Services.appinfo.OS === "WINNT") {
    // On Windows, replace forward slashes with backslashes
    return path.replace(/\//g, "\\");
  }

  // On Unix-like systems, replace backslashes with forward slashes
  return path.replace(/\\/g, "/");
}

async function perfSetup({ disabled = false, prefs = [] } = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", !disabled],
      ["browser.ml.logLevel", "Error"],
      ["browser.ml.modelCacheTimeout", 1000],
      ["browser.ml.checkForMemory", false],
      ["javascript.options.wasm_lazy_tiering", true],
      ...prefs,
    ],
  });

  const artifactDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}`
  );

  async function pathExists(path) {
    try {
      return await IOUtils.exists(path);
    } catch (e) {
      return false;
    }
  }

  // Stop immediately if this fails.
  if (!artifactDirectory) {
    throw new Error(
      `The wasm artifact directory is not set. This usually happens when running locally. " +
      "Please download all the files from taskcluster/kinds/fetch/onnxruntime-web-fetch.yml. " +
      "Place them in a directory and rerun the test with the environment variable 'MOZ_FETCHES_DIR' " +
      "set such that all the files are directly inside 'MOZ_FETCHES_DIR'`
    );
  }

  if (!PathUtils.isAbsolute(artifactDirectory)) {
    throw new Error(
      "Please provide an absolute path for 'MOZ_FETCHES_DIR and not a relative path"
    );
  }

  async function download(record) {
    const recordPath = normalizePathForOS(
      `${artifactDirectory}/${record.name}`
    );

    // Stop immediately if this fails.
    if (!(await pathExists(recordPath))) {
      throw new Error(`The wasm file <${recordPath}> does not exist. This usually happens when running locally. " +
        "Please download all the files from taskcluster/kinds/fetch/onnxruntime-web-fetch.yml. " +
        "Place them in the directory <${artifactDirectory}> " +
        "such that <${recordPath}> exists.`);
    }

    return {
      buffer: (await IOUtils.read(recordPath)).buffer,
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
      await SpecialPowers.popPrefEnv();
    },
  };
}

/**
 * Returns the total memory usage in MiB for the inference process
 */
async function getTotalMemoryUsage() {
  let mgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager
  );

  let total = 0;

  const handleReport = (
    aProcess,
    aPath,
    _aKind,
    _aUnits,
    aAmount,
    _aDescription
  ) => {
    if (aProcess.startsWith("inference")) {
      if (aPath.startsWith("explicit")) {
        total += aAmount;
      }
    }
  };

  await new Promise(r =>
    mgr.getReports(handleReport, null, r, null, /* anonymized = */ false)
  );

  return Math.round(total / 1024 / 1024);
}

/**
 * Runs an inference given the options and arguments
 *
 */
async function runInference(pipelineOptions, request, isFirstRun = false) {
  const { cleanup, engine } = await initializeEngine(pipelineOptions);
  let metrics = {};
  try {
    const res = await engine.run(request);
    metrics = fetchMetrics(res.metrics, isFirstRun);
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${TOTAL_MEMORY_USAGE}`] =
      await getTotalMemoryUsage();
  } finally {
    await EngineProcess.destroyMLEngine();
    await cleanup();
  }
  return metrics;
}

/**
 * Runs a performance test for the given name, options, and arguments and
 * reports the results for perfherder.
 */
async function perfTest(
  name,
  options,
  request,
  iterations = ITERATIONS,
  addColdStart = false
) {
  name = name.toUpperCase();

  let METRICS = [
    `${name}-${PIPELINE_READY_LATENCY}`,
    `${name}-${INITIALIZATION_LATENCY}`,
    `${name}-${MODEL_RUN_LATENCY}`,
    `${name}-${TOTAL_MEMORY_USAGE}`,
    ...(addColdStart
      ? [
          `${name}-${COLD_START_PREFIX}${PIPELINE_READY_LATENCY}`,
          `${name}-${COLD_START_PREFIX}${INITIALIZATION_LATENCY}`,
          `${name}-${COLD_START_PREFIX}${MODEL_RUN_LATENCY}`,
          `${name}-${COLD_START_PREFIX}${TOTAL_MEMORY_USAGE}`,
        ]
      : []),
  ];

  const journal = {};
  for (let metric of METRICS) {
    journal[metric] = [];
  }

  const pipelineOptions = new PipelineOptions(options);
  let nIterations = addColdStart ? iterations + 1 : iterations;
  for (let i = 0; i < nIterations; i++) {
    const shouldAddColdStart = addColdStart && i === 0;
    let metrics = await runInference(
      pipelineOptions,
      request,
      shouldAddColdStart
    );
    for (let [metricName, metricVal] of Object.entries(metrics)) {
      if (metricVal === null || metricVal === undefined || metricVal < 0) {
        metricVal = 0;
      }
      journal[`${name}-${metricName}`].push(metricVal);
    }
  }
  Assert.ok(true);
  reportMetrics(journal);
}
