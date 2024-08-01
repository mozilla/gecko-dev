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

/**
 * The engine child is responsible for the life cycle and instantiation of the local
 * machine learning inference engine.
 */
export class MLEngineChild extends JSWindowActorChild {
  /**
   * The cached engines.
   *
   * @type {Map<string, EngineDispatcher>}
   */
  #engineDispatchers = new Map();

  // eslint-disable-next-line consistent-return
  async receiveMessage({ name, data }) {
    switch (name) {
      case "MLEngine:NewPort": {
        const { port, pipelineOptions } = data;

        // Override some options using prefs
        let options = new lazy.PipelineOptions(pipelineOptions);

        options.updateOptions({
          modelHubRootUrl: lazy.MODEL_HUB_ROOT_URL,
          modelHubUrlTemplate: lazy.MODEL_HUB_URL_TEMPLATE,
          timeoutMS: lazy.CACHE_TIMEOUT_MS,
          logLevel: lazy.LOG_LEVEL,
        });

        // Check if we already have an engine under this id.
        if (this.#engineDispatchers.has(options.engineId)) {
          let currentEngineDispatcher = this.#engineDispatchers.get(
            options.engineId
          );

          // The option matches, let's reuse the engine
          if (currentEngineDispatcher.pipelineOptions.equals(options)) {
            return;
          }

          // The options do not match, terminate the old one so we have a single engine per id.
          await currentEngineDispatcher.terminate(
            /* shutDownIfEmpty */ false,
            /* replacement */ true
          );
          this.#engineDispatchers.delete(options.engineId);
        }

        this.#engineDispatchers.set(
          options.engineId,
          new EngineDispatcher(this, port, options)
        );
        break;
      }
      case "MLEngine:ForceShutdown": {
        for (const engineDispatcher of this.#engineDispatchers.values()) {
          await engineDispatcher.terminate(
            /* shutDownIfEmpty */ true,
            /* replacement */ false
          );
        }
        this.#engineDispatchers = null;
        break;
      }
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded":
        this.sendAsyncMessage("MLEngine:Ready");
        break;
    }
  }

  /**
   * Gets the wasm array buffer from RemoteSettings.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  getWasmArrayBuffer() {
    return this.sendQuery("MLEngine:GetWasmArrayBuffer");
  }

  /**
   * Gets the inference options from RemoteSettings.
   *
   * @returns {Promise<object>}
   */
  getInferenceOptions(taskName) {
    return this.sendQuery("MLEngine:GetInferenceOptions", {
      taskName,
    });
  }

  /**
   * Retrieves a model file as an ArrayBuffer and headers by communicating with the parent actor.
   *
   * @param {object} config - The configuration accepted by the parent function.
   * @returns {Promise<[ArrayBuffer, object]>} The file content and headers
   */
  getModelFile(config) {
    return this.sendQuery("MLEngine:GetModelFile", config);
  }

  /**
   * Removes an engine by its ID. Optionally shuts down if no engines remain.
   *
   * @param {string} engineId - The ID of the engine to remove.
   * @param {boolean} [shutDownIfEmpty] - If true, shuts down the engine process if no engines remain.
   * @param {boolean} replacement - Flag indicating whether the engine is being replaced.
   */
  removeEngine(engineId, shutDownIfEmpty, replacement) {
    if (!this.#engineDispatchers) {
      return;
    }
    this.#engineDispatchers.delete(engineId);

    this.sendAsyncMessage("MLEngine:Removed", {
      engineId,
      shutdown: shutDownIfEmpty,
      replacement,
    });

    if (this.#engineDispatchers.size === 0 && shutDownIfEmpty) {
      this.sendAsyncMessage("MLEngine:DestroyEngineProcess");
    }
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
  #engineId;

  /** @type {PipelineOptions | null} */
  pipelineOptions = null;

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
    // Create the inference engine given the wasm runtime and the options.
    const wasm = await this.mlEngineChild.getWasmArrayBuffer();
    let remoteSettingsOptions = await this.mlEngineChild.getInferenceOptions(
      this.#taskName
    );

    // Merge the RemoteSettings inference options with the pipeline options provided.
    let mergedOptions = new lazy.PipelineOptions(remoteSettingsOptions);
    mergedOptions.updateOptions(pipelineOptions);
    lazy.console.debug("Inference engine options:", mergedOptions);

    this.pipelineOptions = mergedOptions;

    return InferenceEngine.create({
      wasm,
      pipelineOptions: mergedOptions,
      notificationsCallback,
      getModelFileFn: this.mlEngineChild.getModelFile.bind(this.mlEngineChild),
    });
  }

  /**
   * @param {MLEngineChild} mlEngineChild
   * @param {MessagePort} port
   * @param {PipelineOptions} pipelineOptions
   */
  constructor(mlEngineChild, port, pipelineOptions) {
    this.mlEngineChild = mlEngineChild;
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

  handleInitProgressStatus(port, notificationsData) {
    port.postMessage({
      type: "EnginePort:InitProgress",
      statusResponse: notificationsData,
    });
  }

  /**
   * The worker needs to be shutdown after some amount of time of not being used.
   */
  keepAlive() {
    if (this.#keepAliveTimeout) {
      // Clear any previous timeout.
      lazy.clearTimeout(this.#keepAliveTimeout);
    }
    // In automated tests, the engine is manually destroyed.
    if (!Cu.isInAutomation) {
      this.#keepAliveTimeout = lazy.setTimeout(
        this.terminate.bind(this),
        this.timeoutMS
      );
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
          const { requestId, request } = data;
          let engine;
          try {
            engine = await this.#engine;
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

          try {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: await engine.run(request),
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
    try {
      const engine = await this.#engine;
      engine.terminate();
    } catch (error) {
      lazy.console.error("Failed to get the engine", error);
    }

    this.mlEngineChild.removeEngine(
      this.#engineId,
      shutDownIfEmpty,
      replacement
    );
  }
}

/**
 * Wrapper for a function that fetches a model file as an ArrayBuffer from a specified URL and task name.
 *
 * @param {object} config
 * @param {string} config.taskName - name of the inference task.
 * @param {string} config.url - The URL of the model file to fetch. Can be a path relative to
 * the model hub root or an absolute URL.
 * @param {?function(object):Promise<[ArrayBuffer, object]>} config.getModelFileFn - A function that actually retrieves the model data and headers.
 * @returns {Promise} A promise that resolves to a Meta object containing the URL, response headers,
 * and data as an ArrayBuffer. The data is marked for transfer to avoid cloning.
 */
async function getModelFile({ taskName, url, getModelFileFn }) {
  const [data, headers] = await getModelFileFn({
    taskName,
    url,
    rootUrl: lazy.MODEL_HUB_ROOT_URL,
    urlTemplate: lazy.MODEL_HUB_URL_TEMPLATE,
  });
  return new lazy.BasePromiseWorker.Meta([url, headers, data], {
    transfers: [data],
  });
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
   * @param {ArrayBuffer} config.wasm
   * @param {PipelineOptions} config.pipelineOptions
   * @param {?function(ProgressAndStatusCallbackParams):void} config.notificationsCallback The callback to call for updating about notifications such as dowload progress status.
   * @param {?function(object):Promise<[ArrayBuffer, object]>} config.getModelFileFn - A function that actually retrieves the model data and headers.
   * @returns {InferenceEngine}
   */
  static async create({
    wasm,
    pipelineOptions,
    notificationsCallback, // eslint-disable-line no-unused-vars
    getModelFileFn,
  }) {
    /** @type {BasePromiseWorker} */
    const worker = new lazy.BasePromiseWorker(
      "chrome://global/content/ml/MLEngine.worker.mjs",
      { type: "module" },
      {
        getModelFile: async url =>
          getModelFile({
            url,
            taskName: pipelineOptions.taskName,
            getModelFileFn,
          }),
      }
    );

    const args = [wasm, pipelineOptions];
    const closure = {};
    const transferables = [wasm];
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
   * @returns {Promise<string>}
   */
  run(request) {
    return this.#worker.post("run", [request]);
  }

  terminate() {
    if (this.#worker) {
      this.#worker.terminate();
      this.#worker = null;
    }
  }
}
