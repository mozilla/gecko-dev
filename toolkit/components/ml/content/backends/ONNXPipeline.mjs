/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The following globals are defined in dom/webidl/ONNX.webidl
/* global Tensor, InferenceSession */

/**
 * @typedef {import("../../content/Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
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
    setLogLevel: "chrome://global/content/ml/Utils.sys.mjs",
  },
  { global: "current" }
);

const NATIVE_BACKEND = "onnx-native";
const WASM_BACKEND = "onnx";

/**
 * A global reference to the Transformers library.
 * Initially `null` until `importTransformers` sets it.
 *
 * @global
 * @type {object | null}
 */
let transformers = null;

/**
 * Conditionally imports the Transformers library (transformers.js or transformers-dev.js)
 * on first usage, depending on the environment. If the "onnx-native" backend is used,
 * it exposes the `onnxruntime` object on the global scope under `Symbol.for("onnxruntime")`.
 *
 * @async
 * @function importTransformers
 * @param {string} backend - The backend to use (e.g. "onnx-native" or "onnx").
 * @returns {Promise<void>} A promise that resolves once the Transformers library is imported.
 */
async function importTransformers(backend) {
  if (transformers) {
    return;
  }

  lazy.console.debug(`Using backend ${backend}`);

  if (backend === NATIVE_BACKEND) {
    // check if we have the native backend.
    if (typeof InferenceSession === "undefined") {
      throw new Error("onnx-native backend not supported");
    }

    // Exposing an onnxruntime object to the Transformers lib.
    const onnxruntime = {
      InferenceSession,
      Tensor,
      supportedDevices: ["cpu"],
      defaultDevices: ["cpu"],
    };
    globalThis[Symbol.for("onnxruntime")] = onnxruntime;
  }
  if (AppConstants.NIGHTLY_BUILD) {
    lazy.console.debug("Nightly detected. Using transformers-dev.js");
    transformers = await import(
      "chrome://global/content/ml/transformers-dev.js"
    );
  } else {
    lazy.console.debug("Beta or Release detected, using transformers.js");
    transformers = await import("chrome://global/content/ml/transformers.js");
  }
}

/**
 * Apply logit processor for bad words. This is a patch to a bug with bad words processing in transformers.js
 * https://github.com/huggingface/transformers.js/pull/1278/files
 * // TODO remove once Transformers 3.4.3+ is vendored
 *
 * @param {bigint[][]} input_ids The input IDs.
 * @param {Tensor} logits The logits.
 * @returns {Tensor} The processed logits.
 */
function badWordsProcessorPatchWithBugFix(input_ids, logits) {
  for (let i = 0; i < input_ids.length; ++i) {
    const batch_logits_data = /** @type {Float32Array} */ (logits[i].data);
    const ids = input_ids[i];
    for (const bad_word_ids of this.bad_words_ids) {
      // There aren't enough tokens to match the banned sequence
      if (ids.length < bad_word_ids.length - 1) {
        continue;
      }
      // Whether to modify the logits of the last token in the bad word id sequence
      let mark = true;

      // For each bad word in the list, if the current sequence of input ids ends with this sequence (excluding the last),
      // then we set the logits of the last bad word id to -Infinity.
      for (let j = 1; j <= bad_word_ids.length - 1; ++j) {
        // NOTE: We use != instead of !== to compare bigint and number
        // @ts-ignore
        if (bad_word_ids.at(-j - 1) != ids.at(-j)) {
          // We have found a mismatch
          mark = false;
          break;
        }
      }
      if (mark) {
        batch_logits_data[bad_word_ids.at(-1)] = -Infinity;
      }
    }
  }
  return logits;
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
export class ONNXPipeline {
  #mlEngineWorker = null;
  #model = null;
  #tokenizer = null;
  #processor = null;
  #pipelineFunction = null;
  #genericPipelineFunction = null;
  #isReady = false;
  #config = null;
  #metrics = null;
  #errorFactory = null;

  /**
   * Creates an instance of a Pipeline.
   *
   * @param {object} mlEngineWorker - Implements the Cache interface and used to get models
   * @param {object} config - The configuration options
   * @param {*} errorFactory - error class passed by the backend factory.
   */
  constructor(mlEngineWorker, config, errorFactory) {
    this.#errorFactory = errorFactory;
    this.#mlEngineWorker = mlEngineWorker;
    this.#metrics = [];
    let device;
    let session_options = {};

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

    if (config.backend === WASM_BACKEND) {
      transformers.env.backends.onnx.wasm.numThreads = config.numThreads;

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
      if (config.device === "cpu") {
        config.device = "wasm";
      }
      device = config.device || "wasm";
    } else {
      device = "cpu";
      session_options.intraOpNumThreads = config.numThreads;
      session_options.interOpNumThreads = config.numThreads;
      session_options.execution_mode = "sequential";
    }

    transformers.NoBadWordsLogitsProcessor.prototype._call =
      badWordsProcessorPatchWithBugFix; // Fix bug with bad words filter

    lazy.console.debug("Transformers.js env", transformers.env);

    const dtype = (config.dtype = config.dtype || "q8");
    const modelRevision = (config.modelRevision =
      config.modelRevision || "main");

    lazy.console.debug(
      `Setting up pipeline for ${device} using ${dtype} quantization.`
    );

    if (config.pipelineFunction && config.taskName != "test-echo") {
      lazy.console.debug("Using internal inference function");

      // use the model revision of the tokenizer or processor don't have one
      if (!config.tokenizerRevision) {
        config.tokenizerRevision = modelRevision;
      }
      if (!config.processorRevision) {
        config.processorRevision = modelRevision;
      }

      this.#pipelineFunction = config.pipelineFunction;

      if (config.modelClass && config.modelId) {
        lazy.console.debug(
          `Loading model ${config.modelId} with class ${config.modelClass}`
        );
        this.#model = transformers[config.modelClass].from_pretrained(
          config.modelId,
          {
            revision: modelRevision,
            device,
            dtype,
            use_external_data_format: config.useExternalDataFormat ?? false,
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
          {
            revision: config.modelRevision,
            device,
            dtype,
            use_external_data_format: config.useExternalDataFormat,
            session_options,
          }
        );
      } else {
        this.#genericPipelineFunction = async () => {};
      }
    }
    this.#config = config;
    lazy.console.debug("Pipeline initialized");
  }

  async #metricsSnapShot({ name, snapshot = {} }) {
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
   * @param {*} errorFactory - error class passed by the backend factory.
   * @returns {Promise<Pipeline>} The initialized pipeline instance.
   */
  static async initialize(mlEngineWorker, runtime, options, errorFactory) {
    let snapShot = {
      when: Date.now(),
    };

    if (options.logLevel) {
      _logLevel = options.logLevel;
      lazy.setLogLevel(options.logLevel); // setting Utils log level
    }
    const taskName = options.taskName;
    lazy.console.debug(`Initializing Pipeline for task ${taskName}`);
    let config;

    if (!ENGINE_CONFIGURATION[taskName]) {
      lazy.console.debug(`Unknown internal task ${taskName}`);
      // generic pipeline function
      config = {
        pipelineFunction: null,
      };
    } else {
      // Loading the config defaults for the task
      lazy.console.debug(`Internal task detected ${taskName}`);
      config = { ...ENGINE_CONFIGURATION[taskName] };
    }
    config.runtime = runtime;

    // Overriding the defaults with the options
    options.applyToConfig(config);
    config.backend = config.backend || "onnx";

    await importTransformers(config.backend);

    // reapply logLevel if it has changed.
    if (lazy.console.logLevel != config.logLevel) {
      lazy.console.logLevel = config.logLevel;
    }
    const pipeline = new ONNXPipeline(mlEngineWorker, config, errorFactory);
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
          try {
            this.#genericPipelineFunction = await this.#genericPipelineFunction;
          } catch (error) {
            lazy.console.debug("Error initializing pipeline", error);
            throw this.#errorFactory(error);
          }
        } else {
          lazy.console.debug("Initializing model, tokenizer and processor");

          try {
            [this.#model, this.#tokenizer, this.#processor] = await Promise.all(
              [this.#model, this.#tokenizer, this.#processor]
            );
            this.#isReady = true;
          } catch (error) {
            lazy.console.debug("Error initializing pipeline", error);
            throw this.#errorFactory(error);
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
   * The request may include a `streamerOptions` field to configure streaming behavior.
   * `streamerOptions` is an object with the following properties:
   *
   * - `perTokens` (boolean): If `true`, streams data per token; otherwise, streams per word.
   * - `skipPrompt` (boolean): If `false`, the first returned value will include the prompt.
   * - `returnTokens` (boolean): If `true`, the response will include tokens.
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

    const streamerOptions = {
      perTokens: false,
      skipPrompt: true,
      returnTokens: false,
      ...request.streamerOptions,
    };

    let streamer = undefined;
    let chunkTokens = [];
    let chunkText = "";
    let nextTokensArePrompt = !streamerOptions.skipPrompt;
    let restoreTokenizer = false;

    if (tokenizer && inferenceProgressCallback) {
      const flushPrompts = _tokens => {
        streamer.token_cache = _tokens;
        streamer.end();
        streamer.tokenizer = {
          decode: () => {
            streamer.token_cache = [];
            return "";
          },
        };
        restoreTokenizer = true;
        streamer.next_tokens_are_prompt = false;
      };
      streamer = new transformers.TextStreamer(tokenizer, {
        skip_prompt: streamerOptions.skipPrompt,
        decode_kwargs: {
          skip_special_tokens: true,
        },
        token_callback_function: tokens => {
          if (restoreTokenizer) {
            streamer.tokenizer = tokenizer;
            restoreTokenizer = false;
          }
          if (streamerOptions.perTokens) {
            if (nextTokensArePrompt) {
              flushPrompts(tokens);
            }

            inferenceProgressCallback({
              ...progressInfo,
              metadata: {
                text: chunkText,
                tokens: streamerOptions.returnTokens ? tokens : null,
                isPrompt: nextTokensArePrompt,
                requestId,
              },
              type: lazy.Progress.ProgressType.INFERENCE,
              statusText: lazy.Progress.ProgressStatusText.IN_PROGRESS,
            });

            // We have sent the text, now resetting it
            chunkText = "";
          } else {
            // Append newly received tokens.
            chunkTokens.push(tokens);

            if (nextTokensArePrompt) {
              flushPrompts(tokens);
            }
          }
          nextTokensArePrompt = false;
        },
        // Per-word callback function
        callback_function: text => {
          if (streamerOptions.perTokens) {
            chunkText = text;
          } else {
            inferenceProgressCallback({
              ...progressInfo,
              metadata: {
                text,
                tokens: streamerOptions.returnTokens ? chunkTokens : null,
                requestId,
                isPrompt: nextTokensArePrompt,
              },
              type: lazy.Progress.ProgressType.INFERENCE,
              statusText: lazy.Progress.ProgressStatusText.IN_PROGRESS,
            });
            // reset the chunks.
            chunkTokens = [];
          }
        },
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
        if (result instanceof transformers.Tensor) {
          result = result.tolist();
        }
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
