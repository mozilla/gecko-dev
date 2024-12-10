/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {import("../content/Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
 */

/* eslint-disable-next-line mozilla/reject-import-system-module-from-non-system */
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

/**
 * Log level set by the pipeline.
 *
 * @type {string}
 */
let _logLevel = "Error";

/**
 * Lazy initialization container.
 *
 * @type {object}
 */
const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevel: _logLevel, // we can't use maxLogLevelPref in workers.
    prefix: "ML:ONNXPipeline",
  });
});

ChromeUtils.defineESModuleGetters(
  lazy,
  {
    Progress: "chrome://global/content/ml/Utils.sys.mjs",
  },
  { global: "current" }
);

/**
 * Conditional import for Transformer.js
 *
 * The library will be lazily await on first usage in the Pipeline constructor.
 * If we are in Nightly, we are using the non-minified version.
 */
let transformersPromise;
let transformers = null;
let transformersDev;

if (AppConstants.NIGHTLY_BUILD) {
  transformersPromise = import(
    "chrome://global/content/ml/transformers-dev.js"
  );
  transformersDev = true;
} else {
  transformersPromise = import("chrome://global/content/ml/transformers.js");
  transformersDev = false;
}

/**
 * Echo inference for testing purposes.
 *
 * @async
 * @param {object} request - The request object containing args.
 * @param {object} _model - The model used for inference.
 * @param {object} _tokenizer - The tokenizer used for decoding.
 * @param {object} _processor - The processor used for preparing  data.
 * @param {object} config - The config
 * @returns {Promise<object>} The result object containing the processed text.
 */
async function echo(request, _model, _tokenizer, _processor, config) {
  let result = {};
  for (let key in config) {
    result[key] = String(config[key]);
  }
  result.echo = request.data;

  // Sleeping to simulate inference latency
  await new Promise(resolve => setTimeout(resolve, request.sleepTime ?? 100));

  return {
    metrics: {
      tokenizingTime: 0,
    },
    output: result,
  };
}

/**
 * Converts an image to text using a machine learning model.
 *
 * @async
 * @param {object} request - The request object containing image data.
 * @param {string} [request.url] - The URL of the image to process. If `url` is not provided, other fields are used.
 * @param {ArrayBuffer} [request.data] - The raw image data to process. Ignored if `url` is provided.
 * @param {number} [request.width] - The image width. Ignored if `url` is provided.
 * @param {number} [request.height] - The image height. Ignored if `url` is provided.
 * @param {number} [request.channels] - The image channels. Can be 1, 2, 3 or 4. Defaults to 4. Ignored if `url` is provided.
 * @param {object} model - The model used for inference.
 * @param {object} tokenizer - The tokenizer used for decoding.
 * @param {object} processor - The processor used for preparing image data.
 * @param {object} _config - The config
 * @returns {Promise<object>} The result object containing the processed text.
 */
async function imageToText(request, model, tokenizer, processor, _config) {
  let result = {
    metrics: {
      inferenceTime: 0,
      tokenizingTime: 0,
    },
  };
  let start = Date.now();
  let rawImage;

  if ("url" in request) {
    rawImage = await transformers.RawImage.fromURL(request.url);
  } else {
    rawImage = new transformers.RawImage(
      request.data,
      request.width,
      request.height,
      request.channels || 4
    );
  }

  lazy.console.debug("Image loaded in ", Date.now() - start);

  const { pixel_values } = await processor(rawImage);
  result.metrics.tokenizingTime += Date.now() - start;
  const toReturn = [];
  const streamer = request.options?.streamer;
  for (const batch of pixel_values) {
    batch.dims = [1, ...batch.dims];
    start = Date.now();
    const output = await model.generate({ inputs: batch, streamer });
    result.metrics.inferenceTime += Date.now() - start;
    start = Date.now();
    const decoded = tokenizer
      .batch_decode(output, {
        skip_special_tokens: true,
      })
      .map(x => ({ generated_text: x.trim() }));
    result.metrics.tokenizingTime += Date.now() - start;
    toReturn.push(decoded);
  }
  lazy.console.debug("Inference done in ", Date.now() - start);
  result.output = toReturn[0][0].generated_text;

  // Bug 1918220 - replace the result for models with that bug
  if (result.output === "T") {
    lazy.console.debug("Replacing `T` with `Text document.`");
    result.output = "Text document.";
  }
  return result;
}

/**
 * Configuration for engine. Each task has a configuration object that
 * gets merged at runtime with the options from PipelineOptions.
 *
 * When a key exists in both the default configuration and the options,
 * the value from the options is used.
 *
 * The configuration keys that are not exposed as options are all the
 * callables that are used in the pipeline:
 *
 * - modelClass
 * - tokenizerClass
 * - processorClass
 * - pipelineFunction
 *
 * @type {object}
 */
const ENGINE_CONFIGURATION = {
  "moz-image-to-text": {
    modelId: "mozilla/distilvit",
    modelClass: "AutoModelForVision2Seq",
    tokenizerId: "mozilla/distilvit",
    tokenizerClass: "AutoTokenizer",
    processorId: "mozilla/distilvit",
    processorClass: "AutoProcessor",
    pipelineFunction: imageToText,
  },
  "moz-echo": {
    modelId: null,
    modelClass: null,
    tokenizerId: null,
    tokenizerClass: null,
    processorId: null,
    processorClass: null,
    pipelineFunction: echo,
  },
};

/**
 * Copied over from ONNX see https://github.com/microsoft/onnxruntime
 *
 * This is the code used by the lib to detect multi-thread support.
 *
 */
function isMultiThreadSupported() {
  // If 'SharedArrayBuffer' is not available, WebAssembly threads will not work.
  if (typeof SharedArrayBuffer === "undefined") {
    return false;
  }
  try {
    // Test for transferability of SABs (for browsers. needed for Firefox)
    // https://groups.google.com/forum/#!msg/mozilla.dev.platform/IHkBZlHETpA/dwsMNchWEQAJ
    if (typeof MessageChannel !== "undefined") {
      new MessageChannel().port1.postMessage(new SharedArrayBuffer(1));
    }

    // Test for WebAssembly threads capability (for both browsers and Node.js)
    // This typed array is a WebAssembly program containing threaded instructions.
    //
    // This is the corresponding wasm module generated by wasm2wat:
    // (module
    //  (type (;0;) (func))
    //  (func (;0;) (type 0)
    //    i32.const 0
    //    i32.atomic.load
    //    drop)
    //  (memory (;0;) 1 1 shared))
    return WebAssembly.validate(
      new Uint8Array([
        0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 5, 4, 1, 3,
        1, 1, 10, 11, 1, 9, 0, 65, 0, 254, 16, 2, 0, 26, 11,
      ])
    );
  } catch {
    return false;
  }
}

/**
 * Copied over from ONNX see https://github.com/microsoft/onnxruntime
 *
 * This is the code used by the lib to detect GPU support.
 *
 */
async function checkGPUSupport() {
  if (!navigator?.gpu) {
    return false;
  }

  const adapter = await navigator.gpu.requestAdapter({
    powerPreference: "high-performance",
    forceFallbackAdapter: false,
  });

  return !!adapter;
}

/**
 * Represents a pipeline for processing machine learning tasks.
 */
export class Pipeline {
  #mlEngineWorker = null;
  #model = null;
  #tokenizer = null;
  #processor = null;
  #pipelineFunction = null;
  #genericPipelineFunction = null;
  #isReady = false;
  #config = null;
  #metrics = null;

  /**
   * Creates an instance of a Pipeline.
   *
   * @param {object} mlEngineWorker - Implements the Cache interface and used to get models
   * @param {object} config - The configuration options
   */
  constructor(mlEngineWorker, config) {
    this.#mlEngineWorker = mlEngineWorker;
    this.#metrics = [];
    // Setting up the Transformers.js environment
    // See https://huggingface.co/docs/transformers.js/api/env

    // Caching strategy.
    // Here we make sure that everytime transformers.js requires a file, it uses
    // mlEngineWorker, which transfers the request to the main thread and uses the
    // ModelHub that caches files into IndexDB.
    transformers.env.useBrowserCache = false;
    transformers.env.allowLocalModels = false;
    transformers.env.remoteHost = config.modelHubRootUrl;
    transformers.env.remotePathTemplate = config.modelHubUrlTemplate;
    transformers.env.useCustomCache = true;
    transformers.env.customCache = this.#mlEngineWorker;
    // using `NO_LOCAL` so when the custom cache is used, we don't try to fetch it (see MLEngineWorker.match)
    transformers.env.localModelPath = "NO_LOCAL";

    // numThreads can be set in Remote Settings for the model and we also have a theorical
    // best number of threads based on navigator.hardwareConcurrency and a max of 4.
    // ONNX will take at most 4 threads
    let numThreads = config.numThreads || 0;
    const hardwareConcurrency = navigator.hardwareConcurrency || 1;

    if (numThreads == 0) {
      numThreads = Math.min(4, Math.ceil(hardwareConcurrency / 2));
    } else if (numThreads > hardwareConcurrency) {
      numThreads = hardwareConcurrency;
      lazy.console.warn(
        `numThreads was set equal or higher than hardwareConcurrency, lowering it to ${numThreads}`
      );
    }
    transformers.env.backends.onnx.wasm.numThreads = numThreads;

    // ONNX runtime - we set up the wasm runtime we got from RS for the ONNX backend to pick
    transformers.env.backends.onnx.wasm.wasmPaths = {};
    transformers.env.backends.onnx.wasm.wasmBinary = config.runtime;

    // Set the onnxruntime-web log/verbosity.
    // onnx log levels are "error" | "verbose" | "info" | "warning" | "fatal"
    // the default level is "warning"
    switch (config.logLevel) {
      case "All":
      case "Trace":
        transformers.env.backends.onnx.logLevel = "verbose";
        transformers.env.backends.onnx.trace = true;
        transformers.env.backends.onnx.debug = true;
        break;
      case "Debug":
        transformers.env.backends.onnx.logLevel = "verbose";
        transformers.env.backends.onnx.debug = true;
        break;
      default:
        transformers.env.backends.onnx.logLevel = "warning";
        transformers.env.backends.onnx.trace = false;
        transformers.env.backends.onnx.debug = false;
        break;
    }

    lazy.console.debug("Transformers.js env", transformers.env);

    const device = config.device || "wasm";
    const dtype = config.dtype || "fp32";

    lazy.console.debug(
      `Setting up pipeline for ${device} using ${dtype} quantization.`
    );

    if (config.pipelineFunction && config.taskName != "test-echo") {
      lazy.console.debug("Using internal inference function");

      // use the model revision of the tokenizer or processor don't have one
      if (!config.tokenizerRevision) {
        config.tokenizerRevision = config.modelRevision;
      }
      if (!config.processorRevision) {
        config.processorRevision = config.modelRevision;
      }

      this.#pipelineFunction = config.pipelineFunction;

      if (config.modelClass && config.modelId) {
        lazy.console.debug(
          `Loading model ${config.modelId} with class ${config.modelClass}`
        );
        this.#model = transformers[config.modelClass].from_pretrained(
          config.modelId,
          {
            revision: config.modelRevision,
            device,
            dtype,
          }
        );
      }
      if (config.tokenizerClass && config.tokenizerId) {
        lazy.console.debug(
          `Loading tokenizer ${config.tokenizerId} with class ${config.tokenizerClass}`
        );
        this.#tokenizer = transformers[config.tokenizerClass].from_pretrained(
          config.tokenizerId,
          { revision: config.tokenizerRevision, device, dtype }
        );
      }
      if (config.processorClass && config.processorId) {
        lazy.console.debug(
          `Loading processor ${config.processorId} with class ${config.processorClass}`
        );
        this.#processor = transformers[config.processorClass].from_pretrained(
          config.processorId,
          { revision: config.processorRevision, device, dtype }
        );
      }
    } else {
      lazy.console.debug("Using generic pipeline function");
      if (config.modelId != "test-echo") {
        this.#genericPipelineFunction = transformers.pipeline(
          config.taskName,
          config.modelId,
          { revision: config.modelRevision, device, dtype }
        );
      } else {
        this.#genericPipelineFunction = async () => {};
      }
    }
    this.#config = config;
    lazy.console.debug("Pipeline initialized");
  }

  async #metricsSnapShot({ name, snapshot, collectMemory = true }) {
    if (!snapshot) {
      if (collectMemory) {
        snapshot = await this.#mlEngineWorker.getInferenceProcessInfo();
      } else {
        snapshot = {};
      }
    }
    if (!("when" in snapshot)) {
      snapshot.when = Date.now();
    }
    this.#metrics.push({ name, ...snapshot });
  }

  /**
   * Initializes the pipeline with given options.
   *
   * @static
   * @async
   * @param {object} mlEngineWorker - Implements the Cache interface and used to get models
   * @param {ArrayBuffer} runtime - The runtime wasm file.
   * @param {PipelineOptions} options - The options for initialization.
   * @returns {Promise<Pipeline>} The initialized pipeline instance.
   */
  static async initialize(mlEngineWorker, runtime, options) {
    let snapShot = {
      when: Date.now(),
      ...(await mlEngineWorker.getInferenceProcessInfo()),
    };

    if (options.logLevel) {
      _logLevel = options.logLevel;
    }
    const taskName = options.taskName;
    lazy.console.debug(`Initializing Pipeline for task ${taskName}`);
    let config;

    if (!ENGINE_CONFIGURATION[taskName]) {
      lazy.console.debug(`Unknown internal task ${taskName}`);
      // generic pipeline function
      config = {
        pipelineFunction: null,
        taskName,
        modelId: options.modelId,
        modelRevision: options.modelRevision || "default",
        dtype: options.dtype || "fp16",
        device: options.device || "wasm",
      };
    } else {
      // Loading the config defaults for the task
      lazy.console.debug(`Internal task detected ${taskName}`);
      config = { ...ENGINE_CONFIGURATION[taskName] };
    }
    config.runtime = runtime;

    // Overriding the defaults with the options
    options.applyToConfig(config);

    if (!transformers) {
      if (transformersDev) {
        lazy.console.debug("Nightly detected. Using transformers-dev.js");
      } else {
        lazy.console.debug("Beta or Release detected, using transformers.js");
      }
      transformers = await transformersPromise;
    }

    // reapply logLevel if it has changed.
    if (lazy.console.logLevel != config.logLevel) {
      lazy.console.logLevel = config.logLevel;
    }
    const pipeline = new Pipeline(mlEngineWorker, config);
    await pipeline.ensurePipelineIsReady();
    await pipeline.#metricsSnapShot({
      name: "initializationStart",
      snapshot: snapShot,
    });
    await pipeline.#metricsSnapShot({ name: "initializationEnd" });

    return pipeline;
  }

  /**
   * Ensure all promises are resolved to complete file downloads and model initialization in memory.
   */
  async ensurePipelineIsReady() {
    if (!this.#isReady) {
      if (this.#config.device === "gpu") {
        if (!(await checkGPUSupport())) {
          lazy.console.warn(
            "GPU not supported. ONNX will fallback to CPU instead."
          );
        }
      }
      // deactive console.warn, see https://bugzilla.mozilla.org/show_bug.cgi?id=1891003
      const originalWarn = console.warn;
      await this.#metricsSnapShot({ name: "ensurePipelineIsReadyStart" });

      console.warn = () => {};
      try {
        if (
          this.#genericPipelineFunction &&
          this.#config.modelId != "test-echo"
        ) {
          lazy.console.debug("Initializing pipeline");
          this.#genericPipelineFunction = await this.#genericPipelineFunction;
        } else {
          lazy.console.debug("Initializing model, tokenizer and processor");

          try {
            [this.#model, this.#tokenizer, this.#processor] = await Promise.all(
              [this.#model, this.#tokenizer, this.#processor]
            );
            this.#isReady = true;
          } catch (error) {
            lazy.console.debug("Error initializing pipeline", error);
            throw error;
          }
        }
      } finally {
        console.warn = originalWarn;
      }
      await this.#metricsSnapShot({ name: "ensurePipelineIsReadyEnd" });
      lazy.console.debug("Pipeline is fully initialized");
    }
  }

  /**
   * Runs the pipeline with the given request.
   *
   * @async
   * @param {T} request - The request object to be processed. The fields it may contain
   * depends on the task. See each pipeline function for more details.
   * When the pipeline is initialized with the generic pipeline function, the request contains
   * `args` and `options` fields. The `args` field is an array of values that are passed
   * to the generic pipeline function. The `options` field is an object that contains the options for the pipeline.
   * @param {string} requestId - The identifier used to internally track this request.
   * @param {?function(ProgressAndStatusCallbackParams):void} inferenceProgressCallback A function to call to indicate inference progress.
   * @returns {Promise<object>} The result object from the pipeline execution.
   */
  async run(request, requestId, inferenceProgressCallback = null) {
    lazy.console.debug("Running task: ", this.#config.taskName);

    let result;
    await this.#metricsSnapShot({ name: "runStart" });

    const tokenizer =
      this.#genericPipelineFunction?.tokenizer ?? this.#tokenizer;

    const progressInfo = {
      ok: true,
      id: request.id ?? requestId,
    };

    let streamer = undefined;

    if (tokenizer && inferenceProgressCallback) {
      streamer = new transformers.TextStreamer(tokenizer, {
        skip_prompt: true,
        decode_kwargs: {
          skip_special_tokens: true,
        },
        callback_function: text =>
          inferenceProgressCallback?.({
            ...progressInfo,
            metadata: {
              text,
              requestId,
            },
            type: lazy.Progress.ProgressType.INFERENCE,
            statusText: lazy.Progress.ProgressStatusText.IN_PROGRESS,
          }),
      });
    }

    // Override streamer in options
    const requestWithCallback = inferenceProgressCallback
      ? {
          ...request,
          options: { ...request.options, streamer },
        }
      : request;

    if (this.#genericPipelineFunction) {
      if (this.#config.modelId === "test-echo") {
        result = {
          output: requestWithCallback.args,
          config: this.#config,
          multiThreadSupported: isMultiThreadSupported(),
        };
      } else {
        result = await this.#genericPipelineFunction(
          ...requestWithCallback.args,
          requestWithCallback.options || {}
        );

        // When the pipeline returns Tensors they are Proxy objects that cannot be cloned.
        // Workaround: convert to JSON and back to JS objects.
        result = JSON.parse(JSON.stringify(result));
      }
    } else {
      result = await this.#pipelineFunction(
        requestWithCallback,
        this.#model,
        this.#tokenizer,
        this.#processor,
        this.#config
      );
    }
    await this.#metricsSnapShot({ name: "runEnd" });
    result.metrics = this.#metrics;

    if (streamer) {
      inferenceProgressCallback?.({
        ...progressInfo,
        metadata: {
          text: "",
          requestId,
        },
        type: lazy.Progress.ProgressType.INFERENCE,
        statusText: lazy.Progress.ProgressStatusText.DONE,
      });
    }

    return result;
  }
}
