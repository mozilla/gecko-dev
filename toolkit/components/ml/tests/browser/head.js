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

/*
 * Perftest related
 */
const MB_TO_BYTES = 1024 * 1024;

const PIPELINE_READY_START = "ensurePipelineIsReadyStart";
const PIPELINE_READY_END = "ensurePipelineIsReadyEnd";
const INIT_START = "initializationStart";
const INIT_END = "initializationEnd";
const RUN_START = "runStart";
const RUN_END = "runEnd";

const PIPELINE_READY_LATENCY = "pipeline-ready-latency";
const INITIALIZATION_LATENCY = "initialization-latency";
const MODEL_RUN_LATENCY = "model-run-latency";
const PIPELINE_READY_MEMORY = "pipeline-ready-memory";
const INITIALIZATION_MEMORY = "initialization-memory";
const MODEL_RUN_MEMORY = "model-run-memory";

const WHEN = "when";
const MEMORY = "memory";

const formatNumber = new Intl.NumberFormat("en-US", {
  maximumSignificantDigits: 4,
}).format;

const median = arr => {
  arr = [...arr].sort((a, b) => a - b);
  const mid = Math.floor(arr.length / 2);

  if (arr.length % 2) {
    return arr[mid];
  }

  return (arr[mid - 1] + arr[mid]) / 2;
};

const stringify = arr => {
  function pad(str) {
    str = str.padStart(7, " ");
    if (str[0] != " ") {
      str = " " + str;
    }
    return str;
  }

  return arr.reduce((acc, elem) => acc + pad(formatNumber(elem)), "");
};

const reportMetrics = journal => {
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
};

const fetchMLMetric = (metrics, name, key) => {
  const metric = metrics.find(metric => metric.name === name);
  return metric[key];
};

const fetchLatencyMetrics = metrics => {
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
    [PIPELINE_READY_LATENCY]: pipelineLatency,
    [INITIALIZATION_LATENCY]: initLatency,
    [MODEL_RUN_LATENCY]: runLatency,
  };
};

const fetchMemoryMetrics = metrics => {
  const pipelineMemory =
    fetchMLMetric(metrics, PIPELINE_READY_END, MEMORY) -
    fetchMLMetric(metrics, PIPELINE_READY_START, MEMORY);
  const initMemory =
    fetchMLMetric(metrics, INIT_END, MEMORY) -
    fetchMLMetric(metrics, INIT_START, MEMORY);
  const runMemory =
    fetchMLMetric(metrics, RUN_END, MEMORY) -
    fetchMLMetric(metrics, RUN_START, MEMORY);
  return {
    [PIPELINE_READY_MEMORY]: pipelineMemory / MB_TO_BYTES,
    [INITIALIZATION_MEMORY]: initMemory / MB_TO_BYTES,
    [MODEL_RUN_MEMORY]: runMemory / MB_TO_BYTES,
  };
};

const fetchMetrics = metrics => {
  return {
    ...fetchLatencyMetrics(metrics),
    ...fetchMemoryMetrics(metrics),
  };
};

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

const runInference = async (pipelineOptions, args) => {
  const modelDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}/onnx-models`
  );
  info(`Model Directory: ${modelDirectory}`);
  const { baseUrl: modelHubRootUrl } = startHttpServer(modelDirectory);
  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  const { cleanup } = await setup({
    prefs: [["browser.ml.modelHubRootUrl", modelHubRootUrl]],
  });
  info("Get the engine process");
  const mlEngineParent = await EngineProcess.getMLEngineParent();

  info("Get Pipeline Options");
  info("Run the inference");
  const engineInstance = await mlEngineParent.getEngine(pipelineOptions);

  const request = {
    args,
    options: { pooling: "mean", normalize: true },
  };

  const res = await engineInstance.run(request);
  let metrics = fetchMetrics(res.metrics);
  info(metrics);
  await EngineProcess.destroyMLEngine();
  await cleanup();
  return metrics;
};

/*
 * Setup utils
 */
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
  const runtime = await createMLWasmRemoteClient(
    autoDownloadFromRemoteSettings
  );
  const options = await createOptionsRemoteClient(records);

  const remoteClients = {
    "ml-onnx-runtime": runtime,
    "ml-inference-options": options,
  };

  MLEngineParent.mockRemoteSettings({
    "ml-onnx-runtime": runtime.client,
    "ml-inference-options": options,
  });

  return {
    async removeMocks() {
      await runtime.client.attachments.deleteAll();
      await runtime.client.db.clear();
      await options.db.clear();
      MLEngineParent.removeMocks();
    },
    remoteClients,
  };
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @returns {AttachmentMock}
 */
async function createMLWasmRemoteClient(autoDownloadFromRemoteSettings) {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-translation-wasm";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(
    metadata,
    Date.now(),
    getDefaultWasmRecords().map(({ name, version }) => ({
      id: crypto.randomUUID(),
      name,
      version,
      last_modified: Date.now(),
      schema: Date.now(),
    }))
  );

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @returns {RemoteSettings}
 */
async function createOptionsRemoteClient(records = null) {
  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-ml-inference-options";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );

  if (!records) {
    records = [
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
    ];
  }

  await client.db.clear();
  await client.db.importChanges({}, Date.now(), records);
  return client;
}
