/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/// <reference path="../../../../../toolkit/components/translations/tests/browser/shared-head.js" />

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/shared-head.js",
  this
);

/**
 * @type {import("../../actors/MLEngineParent.sys.mjs")}
 */
const { MLEngineParent, MLEngine } = ChromeUtils.importESModule(
  "resource://gre/actors/MLEngineParent.sys.mjs"
);

const { ModelHub, TestIndexedDBCache } = ChromeUtils.importESModule(
  "chrome://global/content/ml/ModelHub.sys.mjs"
);

const { getInferenceProcessInfo } = ChromeUtils.importESModule(
  "chrome://global/content/ml/Utils.sys.mjs"
);

const MS_PER_SEC = 1000;
const IndexedDBCache = TestIndexedDBCache;

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

/**
 * Sets up the stage for a test
 *
 */
async function setup({
  disabled = false,
  prefs = [],
  records = null,
  backend,
} = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
    records,
    backend,
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

function getDefaultWasmRecords(backend) {
  return [
    {
      name: MLEngineParent.WASM_FILENAME[
        backend || MLEngineParent.DEFAULT_BACKEND
      ],
      version: MLEngineParent.WASM_MAJOR_VERSION + ".0",
    },
  ];
}

async function createAndMockMLRemoteSettings({
  autoDownloadFromRemoteSettings = false,
  records = null,
  backend,
} = {}) {
  const wasmRecords = getDefaultWasmRecords(backend).map(
    ({ name, version }) => ({
      id: crypto.randomUUID(),
      name,
      version,
      last_modified: Date.now(),
      schema: Date.now(),
    })
  );
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
const ONE_MIB = 1024 * 1024;
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
const PEAK_MEMORY_USAGE = "peak-memory-usage";
const ITERATIONS = 10;
const WHEN = "when";
const MEMORY = "memory";
const E2E_INIT_LATENCY = "e2e-init-latency";
const FIRST_TOKEN_LATENCY = "1st-token-latency";
const DECODING_LATENCY = "decoding-latency";
// Token speeds are apppropriate for comparing the speed of the same model.
const DECODING_TOKEN_SPEED = "decoding-tokenSpeed";
const PROMPT_TOKEN_SPEED = "prompt-tokenSpeed";
// Characters speed is appropriate for comparing the speed of two different models.
const DECODING_CHARACTERS_SPEED = "decoding-charactersSpeed";
const PROMPT_CHARACTERS_SPEED = "prompt-charactersSpeed";

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

async function initializeEngine(pipelineOptions, prefs = null) {
  const modelDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}/onnx-models`
  );
  info(`Model Directory: ${modelDirectory}`);

  const modelHubRootUrl = Services.env.get("MOZ_MODELS_HUB");
  if (!modelHubRootUrl) {
    throw new Error(
      "MOZ_MODELS_HUB is not set, you need to run with --hooks toolkit/components/ml/tests/tools/hook_local_hub.py"
    );
  }

  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  var browserPrefs = [["browser.ml.modelHubRootUrl", modelHubRootUrl]];
  if (prefs) {
    browserPrefs = browserPrefs.concat(prefs);
  }

  const { cleanup } = await perfSetup({
    prefs: browserPrefs,
    backend: pipelineOptions.backend,
  });
  info("Get the engine process");
  const startTime = performance.now();
  const mlEngineParent = await EngineProcess.getMLEngineParent();
  const engine = await mlEngineParent.getEngine(
    new PipelineOptions(pipelineOptions)
  );
  const e2eInitTime = performance.now() - startTime;

  info("Get Pipeline Options");
  info("Run the inference");
  return {
    cleanup,
    engine,
    e2eInitTime,
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

async function perfSetup({ disabled = false, prefs = [], backend } = {}) {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
    backend,
  });

  var finalPrefs = [
    // Enabled by default.
    ["browser.ml.enable", !disabled],
    ["browser.ml.logLevel", "Error"],
    ["browser.ml.modelCacheTimeout", 1000],
    ["browser.ml.checkForMemory", false],
    ["javascript.options.wasm_lazy_tiering", true],
    ...prefs,
  ];

  await SpecialPowers.pushPrefEnv({
    set: finalPrefs,
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
    info(`Downloading record: ${record.name}`);
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
 * Returns the current total physical memory usage in MiB for the inference process
 */
async function getTotalMemoryUsage() {
  const procInfo = await getInferenceProcessInfo();
  return Math.round(procInfo.memory / ONE_MIB);
}

/**
 * Runs an inference given the options and arguments
 *
 */
async function runInference({
  pipelineOptions,
  request,
  isFirstRun = false,
  browserPrefs = null,
}) {
  info(
    `runInference is request null | ${request === null || request === undefined}`
  );
  const { cleanup, engine, e2eInitTime } = await initializeEngine(
    pipelineOptions,
    browserPrefs
  );

  const streamerOptions = {
    perTokens: true,
    skipPrompt: pipelineOptions.taskName !== "text-generation",
    returnTokens: true,
    ...(request.streamerOptions || {}),
  };
  request = { ...request, streamerOptions };

  let metrics = {};
  let timeToFirstToken;
  let startTime;
  let numGeneratedCharacters = 0;
  let numGeneratedTokens = 0;
  let numPromptCharacters = 0;
  if (streamerOptions.skipPrompt && Array.isArray(request?.args)) {
    numPromptCharacters += request.args
      .flat()
      .reduce((sum, item) => sum + (item?.length || 0), 0);
  }
  let numPromptTokens = 0;
  const run = async () => {
    let isFirstTokenReceived = false;
    let result;
    let currentTokenLen = 0;
    let currentCharLen = 0;
    startTime = performance.now();
    const generator = engine.runWithGenerator(request);

    do {
      result = await generator.next();

      currentTokenLen = result.value?.tokens?.flat()?.length || 0;
      currentCharLen = result.value?.text?.length || 0;

      if (result.value?.isPrompt) {
        numPromptCharacters += currentCharLen;
        numPromptTokens += currentTokenLen;
      } else {
        numGeneratedCharacters += currentCharLen;
        numGeneratedTokens += currentTokenLen;
        if (!isFirstTokenReceived) {
          timeToFirstToken = performance.now() - startTime;
          isFirstTokenReceived = true;
          startTime = performance.now();
        }
      }
    } while (!result.done);

    return result.value;
  };

  try {
    const res = await run();
    const decodingTime = performance.now() - startTime;
    metrics = fetchMetrics(res.metrics || [], isFirstRun);
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${TOTAL_MEMORY_USAGE}`] =
      await getTotalMemoryUsage();

    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${E2E_INIT_LATENCY}`] =
      e2eInitTime;
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${FIRST_TOKEN_LATENCY}`] =
      timeToFirstToken;
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${DECODING_LATENCY}`] =
      decodingTime;
    metrics[
      `${isFirstRun ? COLD_START_PREFIX : ""}${DECODING_CHARACTERS_SPEED}`
    ] = numGeneratedCharacters / (decodingTime / MS_PER_SEC);
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${DECODING_TOKEN_SPEED}`] =
      numGeneratedTokens / (decodingTime / MS_PER_SEC);
    metrics[
      `${isFirstRun ? COLD_START_PREFIX : ""}${PROMPT_CHARACTERS_SPEED}`
    ] = numPromptCharacters / (timeToFirstToken / MS_PER_SEC);
    metrics[`${isFirstRun ? COLD_START_PREFIX : ""}${PROMPT_TOKEN_SPEED}`] =
      numPromptTokens / (timeToFirstToken / MS_PER_SEC);
  } finally {
    await engine.terminate();
    await EngineProcess.destroyMLEngine();
    await cleanup();
  }
  return metrics;
}

/**
 * Can be used to track peak memory
 *
 */
class PeakMemoryTracker {
  constructor(interval = 500) {
    this._memory = 0;
    this._intervalId = null;
    this._interval = interval;
  }

  async collectPeakMemory() {
    const procInfo = await getInferenceProcessInfo();
    if (procInfo.memory && procInfo.memory > this._memory) {
      this._memory = procInfo.memory;
    }
  }

  start() {
    if (this._intervalId !== null) {
      return;
    } // Prevent multiple intervals
    this._intervalId = setInterval(() => {
      this.collectPeakMemory().catch(console.error);
    }, this._interval);
  }

  stop() {
    if (this._intervalId !== null) {
      clearInterval(this._intervalId);
      this._intervalId = null;
    }

    try {
      return Math.round(this._memory / ONE_MIB);
    } finally {
      this._memory = 0;
    }
  }
}
/**
 * Runs a performance test for the given name, options, and arguments and
 * reports the results for perfherder.
 */
async function perfTest({
  name,
  options,
  request,
  iterations = ITERATIONS,
  addColdStart = false,
  trackPeakMemory = false,
  peakMemoryInterval = 500,
  browserPrefs = null,
}) {
  info(`is request null | ${request === null || request === undefined}`);
  name = name.toUpperCase();

  let METRICS;

  // When tracking peak memory we only do this because we're
  // stressing the system with 500ms callbacks so other netrics are impacted
  if (trackPeakMemory) {
    METRICS = [`${name}-${PEAK_MEMORY_USAGE}`];
  } else {
    METRICS = [
      `${name}-${PIPELINE_READY_LATENCY}`,
      `${name}-${INITIALIZATION_LATENCY}`,
      `${name}-${MODEL_RUN_LATENCY}`,
      `${name}-${TOTAL_MEMORY_USAGE}`,
      `${name}-${E2E_INIT_LATENCY}`,
      `${name}-${FIRST_TOKEN_LATENCY}`,
      `${name}-${DECODING_LATENCY}`,
      `${name}-${DECODING_CHARACTERS_SPEED}`,
      `${name}-${DECODING_TOKEN_SPEED}`,
      `${name}-${PROMPT_CHARACTERS_SPEED}`,
      `${name}-${PROMPT_TOKEN_SPEED}`,
      ...(addColdStart
        ? [
            `${name}-${COLD_START_PREFIX}${PIPELINE_READY_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${INITIALIZATION_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${MODEL_RUN_LATENCY}`,
            `${name}-${COLD_START_PREFIX}${TOTAL_MEMORY_USAGE}`,
          ]
        : []),
    ];
  }

  const journal = {};
  for (let metric of METRICS) {
    journal[metric] = [];
  }

  const pipelineOptions = new PipelineOptions(options);
  var tracker;

  let nIterations = addColdStart ? iterations + 1 : iterations;
  for (let i = 0; i < nIterations; i++) {
    if (trackPeakMemory) {
      tracker = new PeakMemoryTracker(peakMemoryInterval);
      tracker.start();
    }
    const shouldAddColdStart = addColdStart && i === 0;
    let metrics = await runInference({
      pipelineOptions,
      request,
      isFirstRun: shouldAddColdStart,
      browserPrefs,
    });
    if (trackPeakMemory) {
      journal[`${name}-${PEAK_MEMORY_USAGE}`].push(tracker.stop());
    } else {
      for (let [metricName, metricVal] of Object.entries(metrics)) {
        if (!Number.isFinite(metricVal) || metricVal < 0) {
          metricVal = 0;
        }
        // Add the metric if it wasn't there
        if (journal[`${name}-${metricName}`] === undefined) {
          journal[`${name}-${metricName}`] = [];
        }
        journal[`${name}-${metricName}`].push(metricVal);
      }
    }
  }
  Assert.ok(true);
  reportMetrics(journal);
}

/**
 * Measures floating point value within epsilon tolerance
 */
function isEqualWithTolerance(A, B, epsilon = 0.000001) {
  return Math.abs(Math.abs(A) - Math.abs(B)) < epsilon;
}
