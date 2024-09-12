/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    arrayBufferToBlobURL: "chrome://global/content/ml/Utils.sys.mjs",
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
  for (const batch of pixel_values) {
    batch.dims = [1, ...batch.dims];
    start = Date.now();
    const output = await model.generate(batch);
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
 * Represents a pipeline for processing machine learning tasks.
 */
export class Pipeline {
  #modelCache = null;
  #model = null;
  #tokenizer = null;
  #processor = null;
  #pipelineFunction = null;
  #genericPipelineFunction = null;
  #initTime = 0;
  #isReady = false;
  #config = null;

  /**
   * Creates an instance of a Pipeline.
   *
   * @param {object} modelCache - Implements the Cache interface and used to get models
   * @param {object} config - The configuration options
   */
  constructor(modelCache, config) {
    let start = Date.now();
    this.#modelCache = modelCache;

    // Setting up the Transformers.js environment
    // See https://huggingface.co/docs/transformers.js/api/env

    // Caching strategy.
    // Here we make sure that everytime transformers.js requires a file, it uses
    // modelCache, which transfers the request to the main thread and uses the
    // ModelHub that caches files into IndexDB.
    transformers.env.useBrowserCache = false;
    transformers.env.allowLocalModels = false;
    transformers.env.remoteHost = config.modelHubRootUrl;
    transformers.env.remotePathTemplate = config.modelHubUrlTemplate;
    transformers.env.useCustomCache = true;
    transformers.env.customCache = this.#modelCache;
    // using `NO_LOCAL` so when the custom cache is used, we don't try to fetch it (see MLEngineWorker.match)
    transformers.env.localModelPath = "NO_LOCAL";

    // ONNX runtime - we set up the wasm runtime we got from RS for the ONNX backend to pick
    lazy.console.debug(
      "Setting up ONNX backend for runtime",
      config.runtimeFilename
    );
    transformers.env.backends.onnx.wasm.wasmPaths = {};
    transformers.env.backends.onnx.wasm.wasmPaths[config.runtimeFilename] =
      lazy.arrayBufferToBlobURL(config.runtime);
    lazy.console.debug("Transformers.js env", transformers.env);

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
          }
        );
      }
      if (config.tokenizerClass && config.tokenizerId) {
        lazy.console.debug(
          `Loading tokenizer ${config.tokenizerId} with class ${config.tokenizerClass}`
        );
        this.#tokenizer = transformers[config.tokenizerClass].from_pretrained(
          config.tokenizerId,
          { revision: config.tokenizerRevision }
        );
      }
      if (config.processorClass && config.processorId) {
        lazy.console.debug(
          `Loading processor ${config.processorId} with class ${config.processorClass}`
        );
        this.#processor = transformers[config.processorClass].from_pretrained(
          config.processorId,
          { revision: config.processorRevision }
        );
      }
    } else {
      lazy.console.debug("Using generic pipeline function");
      this.#genericPipelineFunction = transformers.pipeline(
        config.taskName,
        config.modelId,
        { revision: config.modelRevision }
      );
    }
    this.#initTime = Date.now() - start;
    this.#config = config;
    lazy.console.debug("Pipeline initialized, took ", this.#initTime);
  }

  /**
   * Initializes the pipeline with given options.
   *
   * @static
   * @async
   * @param {object} modelCache - Implements the Cache interface and used to get models
   * @param {ArrayBuffer} runtime - The runtime wasm file.
   * @param {PipelineOptions} options - The options for initialization.
   * @returns {Promise<Pipeline>} The initialized pipeline instance.
   */
  static async initialize(modelCache, runtime, options) {
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

    const pipeline = new Pipeline(modelCache, config);
    await pipeline.ensurePipelineIsReady();
    return pipeline;
  }

  /**
   * Ensure all promises are resolved to complete file downloads and model initialization in memory.
   */
  async ensurePipelineIsReady() {
    if (!this.#isReady) {
      let start = Date.now();

      // deactive console.warn, see https://bugzilla.mozilla.org/show_bug.cgi?id=1891003
      const originalWarn = console.warn;
      console.warn = () => {};
      try {
        if (this.#genericPipelineFunction) {
          lazy.console.debug("Initializing pipeline");
          if (this.#config.modelId != "test-echo") {
            this.#genericPipelineFunction = await this.#genericPipelineFunction;
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
            throw error;
          }
        }
      } finally {
        console.warn = originalWarn;
      }

      this.#initTime += Date.now() - start;
      lazy.console.debug(
        "Pipeline is fully initialized, took ",
        this.#initTime
      );
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
   * @returns {Promise<object>} The result object from the pipeline execution.
   */
  async run(request) {
    lazy.console.debug("Running task: ", this.#config.taskName);

    let result;

    if (this.#genericPipelineFunction) {
      if (this.#config.modelId === "test-echo") {
        result = { output: request.args, config: this.#config };
      } else {
        result = await this.#genericPipelineFunction(
          ...request.args,
          request.options || {}
        );

        // When the pipeline returns Tensors they are Proxy objects that cannot be cloned.
        // Workaround: convert to JSON and back to JS objects.
        result = JSON.parse(JSON.stringify(result));
      }
    } else {
      result = await this.#pipelineFunction(
        request,
        this.#model,
        this.#tokenizer,
        this.#processor,
        this.#config
      );
      result.metrics.initTime = this.#initTime;
    }
    return result;
  }
}
