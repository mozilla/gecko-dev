/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/**
 * @typedef {import("../../promiseworker/PromiseWorker.sys.mjs").BasePromiseWorker} BasePromiseWorker
 */

/**
 * @typedef {object} Lazy
 * @typedef {import("../content/Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
 * @property {typeof import("../../promiseworker/PromiseWorker.sys.mjs").BasePromiseWorker} BasePromiseWorker
 * @property {typeof setTimeout} setTimeout
 * @property {typeof clearTimeout} clearTimeout
 */

/** @type {Lazy} */
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BasePromiseWorker: "resource://gre/modules/PromiseWorker.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  PipelineOptions: "chrome://global/content/ml/EngineProcess.sys.mjs",
  DEFAULT_ENGINE_ID: "chrome://global/content/ml/EngineProcess.sys.mjs",
  DEFAULT_MODELS: "chrome://global/content/ml/EngineProcess.sys.mjs",
  WASM_BACKENDS: "chrome://global/content/ml/EngineProcess.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML:EngineChild",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "CACHE_TIMEOUT_MS",
  "browser.ml.modelCacheTimeout"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODEL_HUB_ROOT_URL",
  "browser.ml.modelHubRootUrl"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODEL_HUB_URL_TEMPLATE",
  "browser.ml.modelHubUrlTemplate"
);
XPCOMUtils.defineLazyPreferenceGetter(lazy, "LOG_LEVEL", "browser.ml.logLevel");
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "mlUtils",
  "@mozilla.org/ml-utils;1",
  "nsIMLUtils"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PIPELINE_OVERRIDE_OPTIONS",
  "browser.ml.overridePipelineOptions",
  "{}"
);

const SAFE_OVERRIDE_OPTIONS = [
  "dtype",
  "logLevel",
  "modelRevision",
  "numThreads",
  "processorRevision",
  "timeoutMS",
  "tokenizerRevision",
];

/**
 * The engine child is responsible for the life cycle and instantiation of the local
 * machine learning inference engine.
 */
export class MLEngineChild extends JSProcessActorChild {
  /**
   * The cached engines.
   *
   * @type {Map<string, EngineDispatcher>}
   */
  #engineDispatchers = new Map();

  /**
   * Engine statuses
   *
   * @type {Map<string, string>}
   */
  #engineStatuses = new Map();

  // eslint-disable-next-line consistent-return
  async receiveMessage({ name, data }) {
    switch (name) {
      case "MLEngine:NewPort": {
        await this.#onNewPortCreated(data);
        break;
      }
      case "MLEngine:GetStatus": {
        return this.getStatus();
      }
      case "MLEngine:ForceShutdown": {
        for (const engineDispatcher of this.#engineDispatchers.values()) {
          await engineDispatcher.terminate(
            /* shutDownIfEmpty */ true,
            /* replacement */ false
          );
        }
        break;
      }
    }
  }

  /**
   * Handles the actions to be performed after a new port has been created.
   * Specifically, it ensures that the engine dispatcher is created if not already present,
   * and notifies the parent through the port once the engine dispatcher is ready.
   *
   * @param {object} config - Configuration object.
   * @param {MessagePort} config.port - The port of the channel.
   * @param {PipelineOptions} config.pipelineOptions - The options for the pipeline.
   * @returns {Promise<void>} - A promise that resolves once the necessary actions are complete.
   */
  async #onNewPortCreated({ port, pipelineOptions }) {
    try {
      // We get some default options from the prefs
      let options = new lazy.PipelineOptions({
        modelHubRootUrl: lazy.MODEL_HUB_ROOT_URL,
        modelHubUrlTemplate: lazy.MODEL_HUB_URL_TEMPLATE,
        timeoutMS: lazy.CACHE_TIMEOUT_MS,
        logLevel: lazy.LOG_LEVEL,
      });

      const updatedPipelineOptions =
        this.getUpdatedPipelineOptions(pipelineOptions);
      options.updateOptions(updatedPipelineOptions);
      const engineId = options.engineId;
      this.#engineStatuses.set(engineId, "INITIALIZING");

      // Check if we already have an engine under this id.
      if (this.#engineDispatchers.has(engineId)) {
        let currentEngineDispatcher = this.#engineDispatchers.get(engineId);

        // The option matches, let's reuse the engine
        if (currentEngineDispatcher.pipelineOptions.equals(options)) {
          port.postMessage({
            type: "EnginePort:EngineReady",
            error: null,
          });
          this.#engineStatuses.set(engineId, "READY");

          return;
        }

        // The options do not match, terminate the old one so we have a single engine per id.
        await currentEngineDispatcher.terminate(
          /* shutDownIfEmpty */ false,
          /* replacement */ true
        );
        this.#engineDispatchers.delete(engineId);
      }

      this.#engineStatuses.set(engineId, "CREATING");

      const dispatcher = new EngineDispatcher(this, port, options);
      this.#engineDispatchers.set(engineId, dispatcher);

      // When the pipeline is mocked typically in unit tests, the WASM files are
      // mocked.  In these cases, the pipeline is not resolved during
      // initialization to allow the test to work.
      //
      // NOTE: This is done after adding to #engineDispatchers to ensure other
      // async calls see the new dispatcher.
      if (!lazy.PipelineOptions.isMocked(pipelineOptions)) {
        await dispatcher.ensureInferenceEngineIsReady();
      }

      this.#engineStatuses.set(engineId, "READY");
      port.postMessage({
        type: "EnginePort:EngineReady",
        error: null,
      });
    } catch (error) {
      port.postMessage({
        type: "EnginePort:EngineReady",
        error,
      });
    }
  }

  /**
   * Gets the wasm array buffer from RemoteSettings.
   *
   * @param {string} backend - The ML engine for which the WASM buffer is requested.
   * @returns {Promise<ArrayBuffer>}
   */
  getWasmArrayBuffer(backend) {
    return this.sendQuery("MLEngine:GetWasmArrayBuffer", backend);
  }

  /**
   * Gets the configuration of the worker
   *
   * @returns {Promise<object>}
   */
  getWorkerConfig() {
    return this.sendQuery("MLEngine:GetWorkerConfig");
  }

  /**
   * Gets the inference options from RemoteSettings.
   *
   * @returns {Promise<object>}
   */
  getInferenceOptions(featureId, taskName, modelId) {
    return this.sendQuery("MLEngine:GetInferenceOptions", {
      featureId,
      taskName,
      modelId,
    });
  }

  /**
   * Retrieves a model file and headers by communicating with the parent actor.
   *
   * @param {object} config - The configuration accepted by the parent function.
   * @returns {Promise<[string, object]>} The file local path and headers
   */
  getModelFile(config) {
    return this.sendQuery("MLEngine:GetModelFile", config);
  }

  /**
   * Notify that the model download is completed by communicating with the parent actor.
   *
   * @param {object} config - The configuration accepted by the parent function.
   */
  async notifyModelDownloadComplete(config) {
    this.sendQuery("MLEngine:NotifyModelDownloadComplete", config);
  }

  /**
   * Removes an engine by its ID. Optionally shuts down if no engines remain.
   *
   * @param {string} engineId - The ID of the engine to remove.
   * @param {boolean} [shutDownIfEmpty] - If true, shuts down the engine process if no engines remain.
   * @param {boolean} replacement - Flag indicating whether the engine is being replaced.
   */
  removeEngine(engineId, shutDownIfEmpty, replacement) {
    this.#engineDispatchers.delete(engineId);
    this.#engineStatuses.delete(engineId);

    this.sendAsyncMessage("MLEngine:Removed", {
      engineId,
      shutdown: shutDownIfEmpty,
      replacement,
    });

    if (this.#engineDispatchers.size === 0 && shutDownIfEmpty) {
      this.sendAsyncMessage("MLEngine:DestroyEngineProcess");
    }
  }

  /**
   * Collects information about the current status.
   */
  async getStatus() {
    const statusMap = new Map();

    for (const [key, value] of this.#engineStatuses) {
      if (this.#engineDispatchers.has(key)) {
        statusMap.set(key, this.#engineDispatchers.get(key).getStatus());
      } else {
        // The engine is probably being created
        statusMap.set(key, { status: value });
      }
    }
    return statusMap;
  }

  /**
   * @param {PipelineOptions} pipelineOptions - options that we want to safely override
   * @returns {object} - updated pipeline options
   */
  getUpdatedPipelineOptions(pipelineOptions) {
    const overrideOptionsByFeature = JSON.parse(lazy.PIPELINE_OVERRIDE_OPTIONS);
    const overrideOptions = {};
    if (overrideOptionsByFeature.hasOwnProperty(pipelineOptions.featureId)) {
      for (let key of Object.keys(
        overrideOptionsByFeature[pipelineOptions.featureId]
      )) {
        if (SAFE_OVERRIDE_OPTIONS.includes(key)) {
          overrideOptions[key] =
            overrideOptionsByFeature[pipelineOptions.featureId][key];
        }
      }
    }
    return { ...pipelineOptions, ...overrideOptions };
  }
}

/**
 * This classes manages the lifecycle of an ML Engine, and handles dispatching messages
 * to it.
 */
class EngineDispatcher {
  /** @type {MessagePort | null} */
  #port = null;

  /** @type {TimeoutID | null} */
  #keepAliveTimeout = null;

  /** @type {PromiseWithResolvers} */
  #modelRequest;

  /** @type {Promise<Engine> | null} */
  #engine = null;

  /** @type {string} */
  #taskName;

  /** @type {string} */
  #featureId;

  /** @type {string} */
  #engineId;

  /** @type {PipelineOptions | null} */
  pipelineOptions = null;

  /** @type {string} */
  #status;

  /**
   * Creates the inference engine given the wasm runtime and the run options.
   *
   * The initialization is done in three steps:
   * 1. The wasm runtime is fetched from RS
   * 2. The inference options are fetched from RS and augmented with the pipeline options.
   * 3. The inference engine is created with the wasm runtime and the options.
   *
   * Any exception here will be bubbled up for the constructor to log.
   *
   * @param {PipelineOptions} pipelineOptions
   * @param {?function(ProgressAndStatusCallbackParams):void} notificationsCallback The callback to call for updating about notifications such as dowload progress status.
   * @returns {Promise<Engine>}
   */
  async initializeInferenceEngine(pipelineOptions, notificationsCallback) {
    let remoteSettingsOptions = await this.mlEngineChild.getInferenceOptions(
      this.#featureId,
      this.#taskName,
      pipelineOptions.modelId ?? null
    );

    // Merge the RemoteSettings inference options with the pipeline options provided.
    let mergedOptions = new lazy.PipelineOptions(remoteSettingsOptions);
    mergedOptions.updateOptions(pipelineOptions);

    // If the merged options don't have a modelId and we have a default modelId, we set it
    if (!mergedOptions.modelId) {
      const defaultModelEntry = lazy.DEFAULT_MODELS[this.#taskName];
      if (defaultModelEntry) {
        lazy.console.debug(
          `Using default model ${defaultModelEntry.modelId} for task ${this.#taskName}`
        );
        mergedOptions.updateOptions(defaultModelEntry);
      } else {
        throw new Error(`No default model found for task ${this.#taskName}`);
      }
    }

    lazy.console.debug("Inference engine options:", mergedOptions);
    this.pipelineOptions = mergedOptions;

    // load the wasm if required.
    let wasm = null;
    if (lazy.WASM_BACKENDS.includes(pipelineOptions.backend || "onnx")) {
      wasm = await this.mlEngineChild.getWasmArrayBuffer(
        pipelineOptions.backend
      );
    }

    const workerConfig = await this.mlEngineChild.getWorkerConfig();

    return InferenceEngine.create({
      workerUrl: workerConfig.url,
      workerOptions: workerConfig.options,
      wasm,
      pipelineOptions: mergedOptions,
      notificationsCallback,
      getModelFileFn: this.mlEngineChild.getModelFile.bind(this.mlEngineChild),
      notifyModelDownloadCompleteFn:
        this.mlEngineChild.notifyModelDownloadComplete.bind(this.mlEngineChild),
    });
  }

  /**
   * Private Constructor for an Engine Dispatcher.
   *
   * @param {MLEngineChild} mlEngineChild
   * @param {MessagePort} port
   * @param {PipelineOptions} pipelineOptions
   */
  constructor(mlEngineChild, port, pipelineOptions) {
    this.#status = "CREATED";
    this.mlEngineChild = mlEngineChild;
    this.#featureId = pipelineOptions.featureId;
    this.#taskName = pipelineOptions.taskName;
    this.timeoutMS = pipelineOptions.timeoutMS;
    this.#engineId = pipelineOptions.engineId;

    this.#engine = this.initializeInferenceEngine(
      pipelineOptions,
      notificationsData => {
        this.handleInitProgressStatus(port, notificationsData);
      }
    );

    // Trigger the keep alive timer.
    this.#engine
      .then(() => void this.keepAlive())
      .catch(error => {
        if (
          // Ignore errors from tests intentionally causing errors.
          !error?.message?.startsWith("Intentionally")
        ) {
          lazy.console.error("Could not initalize the engine", error);
        }
      });

    this.#setupMessageHandler(port);
  }

  /**
   * Returns the status of the engine
   */
  getStatus() {
    return {
      status: this.#status,
      options: this.pipelineOptions,
      engineId: this.#engineId,
    };
  }

  /**
   * Resolves the engine to fully initialize it.
   */
  async ensureInferenceEngineIsReady() {
    this.#engine = await this.#engine;
    this.#status = "READY";
  }

  handleInitProgressStatus(port, notificationsData) {
    port.postMessage({
      type: "EnginePort:InitProgress",
      statusResponse: notificationsData,
    });
  }

  /**
   * The worker will be shutdown automatically after some amount of time of not being used, unless:
   *
   * - timeoutMS is set to -1
   */
  keepAlive() {
    if (this.#keepAliveTimeout) {
      // Clear any previous timeout.
      lazy.clearTimeout(this.#keepAliveTimeout);
    }
    if (this.timeoutMS >= 0) {
      this.#keepAliveTimeout = lazy.setTimeout(
        this.terminate.bind(
          this,
          /* shutDownIfEmpty */ true,
          /* replacement */ false
        ),
        this.timeoutMS
      );
    } else {
      this.#keepAliveTimeout = null;
    }
  }

  /**
   * @param {MessagePort} port
   */
  getModel(port) {
    if (this.#modelRequest) {
      // There could be a race to get a model, use the first request.
      return this.#modelRequest.promise;
    }
    this.#modelRequest = Promise.withResolvers();
    port.postMessage({ type: "EnginePort:ModelRequest" });
    return this.#modelRequest.promise;
  }

  /**
   * @param {MessagePort} port
   */
  #setupMessageHandler(port) {
    this.#port = port;
    port.onmessage = async ({ data }) => {
      switch (data.type) {
        case "EnginePort:Discard": {
          port.close();
          this.#port = null;
          break;
        }
        case "EnginePort:Terminate": {
          await this.terminate(data.shutdown, data.replacement);
          break;
        }
        case "EnginePort:ModelResponse": {
          if (this.#modelRequest) {
            const { model, error } = data;
            if (model) {
              this.#modelRequest.resolve(model);
            } else {
              this.#modelRequest.reject(error);
            }
            this.#modelRequest = null;
          } else {
            lazy.console.error(
              "Got a EnginePort:ModelResponse but no model resolvers"
            );
          }
          break;
        }
        case "EnginePort:Run": {
          const { requestId, request, engineRunOptions } = data;
          try {
            await this.ensureInferenceEngineIsReady();
          } catch (error) {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: null,
              error,
            });
            // The engine failed to load. Terminate the entire dispatcher.
            await this.terminate(
              /* shutDownIfEmpty */ true,
              /* replacement */ false
            );
            return;
          }

          // Do not run the keepAlive timer until we are certain that the engine loaded,
          // as the engine shouldn't be killed while it is initializing.
          this.keepAlive();

          this.#status = "RUNNING";
          try {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: await this.#engine.run(
                request,
                requestId,
                engineRunOptions
              ),
              error: null,
            });
          } catch (error) {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: null,
              error,
            });
          }
          this.#status = "IDLING";
          break;
        }
        default:
          lazy.console.error("Unknown port message to engine: ", data);
          break;
      }
    };
  }

  /**
   * Terminates the engine and its worker after a timeout.
   *
   * @param {boolean} shutDownIfEmpty - If true, shuts down the engine process if no engines remain.
   * @param {boolean} replacement - Flag indicating whether the engine is being replaced.
   */
  async terminate(shutDownIfEmpty, replacement) {
    if (this.#keepAliveTimeout) {
      lazy.clearTimeout(this.#keepAliveTimeout);
      this.#keepAliveTimeout = null;
    }
    if (this.#port) {
      // This call will trigger back an EnginePort:Discard that will close the port
      this.#port.postMessage({ type: "EnginePort:EngineTerminated" });
    }

    this.#status = "TERMINATING";
    try {
      const engine = await this.#engine;
      engine.terminate();
    } catch (error) {
      lazy.console.error("Failed to get the engine", error);
    }
    this.#status = "TERMINATED";

    this.mlEngineChild.removeEngine(
      this.#engineId,
      shutDownIfEmpty,
      replacement
    );
  }
}

/**
 * Wrapper for a function that fetches a model file from a specified URL and task name.
 *
 * @param {object} config
 * @param {string} config.engineId - The engine id - defaults to "default-engine".
 * @param {string} config.taskName - name of the inference task.
 * @param {string} config.url - The URL of the model file to fetch. Can be a path relative to
 * the model hub root or an absolute URL.
 * @param {string} config.modelHubRootUrl - root url of the model hub. When not provided, uses the default from prefs.
 * @param {string} config.modelHubUrlTemplate - url template of the model hub. When not provided, uses the default from prefs.
 * @param {?function(object):Promise<[string, object]>} config.getModelFileFn - A function that actually retrieves the model and headers.
 * @param {string} config.featureId - The feature id
 * @param {string} config.sessionId - Shared across the same session.
 * @param {object} config.telemetryData - Additional telemetry data.
 * @returns {Promise} A promise that resolves to a Meta object containing the URL, response headers,
 * and model path.
 */
async function getModelFile({
  engineId,
  taskName,
  url,
  getModelFileFn,
  modelHubRootUrl,
  modelHubUrlTemplate,
  featureId,
  sessionId,
  telemetryData,
}) {
  const [data, headers] = await getModelFileFn({
    engineId: engineId || lazy.DEFAULT_ENGINE_ID,
    taskName,
    url,
    rootUrl: modelHubRootUrl || lazy.MODEL_HUB_ROOT_URL,
    urlTemplate: modelHubUrlTemplate || lazy.MODEL_HUB_URL_TEMPLATE,
    featureId,
    sessionId,
    telemetryData,
  });
  return new lazy.BasePromiseWorker.Meta([url, headers, data], {});
}

/**
 * Wrapper around the ChromeWorker that runs the inference.
 */
class InferenceEngine {
  /** @type {BasePromiseWorker} */
  #worker;

  /**
   * Initialize the worker.
   *
   * @param {object} config
   * @param {string} config.workerUrl  The url of the worker
   * @param {object} config.workerOptions the options to pass to BasePromiseWorker
   * @param {ArrayBuffer} config.wasm
   * @param {PipelineOptions} config.pipelineOptions
   * @param {?function(ProgressAndStatusCallbackParams):void} config.notificationsCallback The callback to call for updating about notifications such as dowload progress status.
   * @param {?function(object):Promise<[string, object]>} config.getModelFileFn - A function that actually retrieves the model and headers.
   * @param {?function(object):Promise<void>} config.notifyModelDownloadCompleteFn - A function to notify that all files needing downloads are completed.
   * @returns {InferenceEngine}
   */
  static async create({
    workerUrl,
    workerOptions,
    wasm,
    pipelineOptions,
    notificationsCallback, // eslint-disable-line no-unused-vars
    getModelFileFn,
    notifyModelDownloadCompleteFn,
  }) {
    // Check for the numThreads value. If it's not set, use the best value for the platform, which is the number of physical cores
    pipelineOptions.numThreads =
      pipelineOptions.numThreads || lazy.mlUtils.getOptimalCPUConcurrency();

    /** @type {BasePromiseWorker} */
    const worker = new lazy.BasePromiseWorker(workerUrl, workerOptions, {
      getModelFile: async (url, sessionId = "") =>
        getModelFile({
          engineId: pipelineOptions.engineId,
          url,
          taskName: pipelineOptions.taskName,
          getModelFileFn,
          modelHubRootUrl: pipelineOptions.modelHubRootUrl,
          modelHubUrlTemplate: pipelineOptions.modelHubUrlTemplate,
          featureId: pipelineOptions.featureId,
          sessionId,
          // We have model, revision that are parsed for the url.
          // However, we want to save in telemetry the ones that are configured
          // for the pipeline. This allows consistent reporting regarding of how
          // the backend constructs the url.
          telemetryData: {
            modelId: pipelineOptions.modelId,
            modelRevision: pipelineOptions.modelRevision,
          },
        }),
      onInferenceProgress: notificationsCallback,
      notifyModelDownloadComplete: async (sessionId = "") =>
        notifyModelDownloadCompleteFn({
          sessionId,
          featureId: pipelineOptions.featureId,
          engineId: pipelineOptions.engineId,
          modelId: pipelineOptions.modelId,
          modelRevision: pipelineOptions.modelRevision,
        }),
    });

    const args = [wasm, pipelineOptions];
    const closure = {};
    const transferables = wasm instanceof ArrayBuffer ? [wasm] : [];
    await worker.post("initializeEngine", args, closure, transferables);
    return new InferenceEngine(worker);
  }

  /**
   * @param {BasePromiseWorker} worker
   */
  constructor(worker) {
    this.#worker = worker;
  }

  /**
   * @param {string} request
   * @param {string} requestId - The identifier used to internally track this request.
   * @param {object} engineRunOptions - Additional run options for the engine.
   * @param {boolean} engineRunOptions.enableInferenceProgress - Whether to enable inference progress.
   * @returns {Promise<string>}
   */
  run(request, requestId, engineRunOptions) {
    return this.#worker.post("run", [request, requestId, engineRunOptions]);
  }

  terminate() {
    if (this.#worker) {
      this.#worker.terminate();
      this.#worker = null;
    }
  }
}
