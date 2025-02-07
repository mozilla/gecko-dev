/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {import("../actors/MLEngineParent.sys.mjs").MLEngineParent} MLEngineParent
 * @typedef {import("../content/Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
 */

/**
 * @constant
 * @type {string}
 * @default
 * @description The default engine identifier used when no specific engine ID is provided.
 */
export const DEFAULT_ENGINE_ID = "default-engine";

/**
 * @constant
 * @type {{ [key: string]: string }}
 * @description Supported tasks with their default model identifiers.
 */
export const DEFAULT_MODELS = Object.freeze({
  "test-echo": { modelId: "test-echo", dtype: "q8" },
  "text-classification": {
    modelId: "Xenova/distilbert-base-uncased-finetuned-sst-2-english",
    dtype: "q8",
  },
  "token-classification": {
    modelId: "Xenova/bert-base-multilingual-cased-ner-hrl",
    dtype: "q8",
  },
  "question-answering": {
    modelId: "Xenova/distilbert-base-cased-distilled-squad",
    dtype: "q8",
  },
  "fill-mask": { modelId: "Xenova/bert-base-uncased", dtype: "q8" },
  summarization: { modelId: "Xenova/distilbart-cnn-6-6", dtype: "q8" },
  translation: { modelId: "Xenova/t5-small", dtype: "q8" },
  "text2text-generation": { modelId: "Xenova/flan-t5-small", dtype: "q8" },
  "text-generation": { modelId: "Xenova/gpt2", dtype: "q8" },
  "zero-shot-classification": {
    modelId: "Xenova/distilbert-base-uncased-mnli",
    dtype: "q8",
  },
  "image-to-text": { modelId: "Mozilla/distilvit", dtype: "q8" },
  "image-classification": {
    modelId: "Xenova/vit-base-patch16-224",
    dtype: "q8",
  },
  "image-segmentation": {
    modelId: "Xenova/detr-resnet-50-panoptic",
    dtype: "q8",
  },
  "zero-shot-image-classification": {
    modelId: "Xenova/clip-vit-base-patch32",
    dtype: "q8",
  },
  "object-detection": { modelId: "Xenova/detr-resnet-50", dtype: "q8" },
  "zero-shot-object-detection": {
    modelId: "Xenova/owlvit-base-patch32",
    dtype: "q8",
  },
  "document-question-answering": {
    modelId: "Xenova/donut-base-finetuned-docvqa",
    dtype: "q8",
  },
  "image-to-image": {
    modelId: "Xenova/swin2SR-classical-sr-x2-64",
    dtype: "q8",
  },
  "depth-estimation": { modelId: "Xenova/dpt-large", dtype: "q8" },
  "feature-extraction": {
    modelId: "Xenova/all-MiniLM-L6-v2",
    dtype: "q8",
  },
  "image-feature-extraction": {
    modelId: "Xenova/vit-base-patch16-224-in21k",
    dtype: "q8",
  },
  "text-to-speech": {
    modelId: "Xenova/speecht5_tts",
    dtype: "q8",
  },
});

/**
 * Lists Firefox internal features
 */
const FEATURES = [
  "autofill-classification", // see toolkit/components/formautofill/MLAutofill.sys.mjs
  "pdfjs-alt-text", // see toolkit/components/pdfjs/content/PdfjsParent.sys.mjs
  "suggest-intent-classification", // see browser/components/urlbar/private/MLSuggest.sys.mjs
  "suggest-NER", // see browser/components/urlbar/private/MLSuggest.sys.mjs
];

/**
 * Custom error class for validation errors.
 *
 * This error is thrown when a field fails validation, providing additional context such as
 * the name of the field that caused the error.
 *
 * @augments Error
 */
class PipelineOptionsValidationError extends Error {
  /**
   * Create a PipelineOptionsValidationError.
   *
   * @param {string} field - The name of the field that caused the validation error.
   * @param {any} value - The invalid value provided for the field.
   * @param {string} [tips=null] - Optional tips or suggestions for valid values.
   */
  constructor(field, value, tips = null) {
    const baseMessage = `Invalid value "${value}" for field "${field}".`;
    const message = tips ? `${baseMessage} ${tips}` : baseMessage;
    super(message);

    this.name = this.constructor.name;
    this.field = field;
    this.value = value;
  }
}

/**
 * Enum for model hubs
 *
 * @readonly
 * @enum {string}
 */
export const ModelHub = {
  HUGGINGFACE: "huggingface",
  MOZILLA: "mozilla",

  apply(options, hub) {
    switch (hub) {
      case ModelHub.HUGGINGFACE:
        options.modelHubRootUrl = "https://huggingface.co/";
        options.modelHubUrlTemplate = "{model}/resolve/{revision}";
        options.modelRevision = "main";
        break;
      case ModelHub.MOZILLA:
        options.modelHubRootUrl = "https://model-hub.mozilla.org/";
        options.modelHubUrlTemplate = "{model}/{revision}";
        options.modelRevision = "main";
        break;
      default:
        throw new Error(`Unknown model hub: ${hub}`);
    }
  },
};

/**
 * Enum for execution priority.
 *
 * Defines the priority of the task:
 *
 * - "High" is absolutely needed for Firefox.
 * - "Normal" is the default priority.
 * - "Low" is for 3rd party calls.
 *
 * @readonly
 * @enum {string}
 */
export const ExecutionPriority = {
  HIGH: "HIGH",
  NORMAL: "NORMAL",
  LOW: "LOW",
};

/**
 * Enum for quantization levels.
 *
 * Defines the quantization level of the task:
 *
 * - 'fp32': Full precision 32-bit floating point (`''`)
 * - 'fp16': Half precision 16-bit floating point (`'_fp16'`)
 * - 'q8': Quantized 8-bit (`'_quantized'`)
 * - 'int8': Integer 8-bit quantization (`'_int8'`)
 * - 'uint8': Unsigned integer 8-bit quantization (`'_uint8'`)
 * - 'q4': Quantized 4-bit (`'_q4'`)
 * - 'bnb4': Binary/Boolean 4-bit quantization (`'_bnb4'`)
 * - 'q4f16': 16-bit floating point model with 4-bit block weight quantization (`'_q4f16'`)
 *
 * @readonly
 * @enum {string}
 */
export const QuantizationLevel = {
  FP32: "fp32",
  FP16: "fp16",
  Q8: "q8",
  INT8: "int8",
  UINT8: "uint8",
  Q4: "q4",
  BNB4: "bnb4",
  Q4F16: "q4f16",
};

/**
 * Enum for the device used for inference.
 *
 * @readonly
 * @enum {string}
 */
export const InferenceDevice = {
  GPU: "gpu",
  WASM: "wasm",
};

/**
 * Enum for log levels.
 *
 * @readonly
 * @enum {string}
 */
export const LogLevel = {
  TRACE: "Trace",
  INFO: "Info",
  DEBUG: "Debug",
  WARN: "Warn",
  ERROR: "Error",
  CRITICAL: "Critical",
  ALL: "All",
};

/**
 * @typedef {import("../../translations/actors/TranslationsEngineParent.sys.mjs").TranslationsEngineParent} TranslationsEngineParent
 */

const PIPELINE_TEST_NAMES = ["moz-echo", "test-echo"];

/**
 * This class encapsulates the options for a pipeline process.
 */
export class PipelineOptions {
  /**
   * External model data file list.
   */
  useExternalDataFormat = false;
  /**
   * The identifier for the engine to be used by the pipeline.
   *
   * @type {?string}
   */
  engineId = DEFAULT_ENGINE_ID;

  /**
   * The name of the feature to be used by the pipeline.
   *
   * This field can be used to uniquely identify an inference and
   * overwrite taskName when doing lookups in Remote Settings.
   *
   * @type {?string}
   */
  featureId = null;

  /**
   * The name of the task the pipeline is configured for.
   *
   * @type {?string}
   */
  taskName = null;

  /**
   * The maximum amount of time in milliseconds the pipeline should wait for a response.
   *
   * @type {?number}
   */
  timeoutMS = null;

  /**
   * The hub to use. When null, looks at modelHubRootUrl and modelHubUrlTemplate
   *
   * @type {ModelHub | null}
   */
  modelHub = null;

  /**
   * The root URL of the model hub where models are hosted.
   *
   * @type {?string}
   */
  modelHubRootUrl = null;

  /**
   * A template URL for building the full URL for the model.
   *
   * @type {?string}
   */
  modelHubUrlTemplate = null;

  /**
   * The identifier for the specific model to be used by the pipeline.
   *
   * @type {?string}
   */
  modelId = null;

  /**
   * The revision for the specific model to be used by the pipeline.
   *
   * @type {?string}
   */
  modelRevision = null;

  /**
   * The identifier for the tokenizer associated with the model, used for pre-processing inputs.
   *
   * @type {?string}
   */
  tokenizerId = null;

  /**
   * The revision for the tokenizer associated with the model, used for pre-processing inputs.
   *
   * @type {?string}
   */
  tokenizerRevision = null;

  /**
   * The identifier for any processor required by the model, used for additional input processing.
   *
   * @type {?string}
   */
  processorId = null;

  /**
   * The revision for any processor required by the model, used for additional input processing.
   *
   * @type {?string}
   */

  processorRevision = null;

  /**
   * The log level used in the worker
   *
   * @type {LogLevel | null}
   */
  logLevel = null;

  /**
   * Name of the runtime wasm file
   *
   * @type {?string}
   */
  runtimeFilename = null;

  /**
   * Device used for inference
   *
   * @type {InferenceDevice | null}
   */
  device = null;

  /**
   * Quantization level
   *
   * @type {QuantizationLevel | null}
   */
  dtype = null;

  /**
   * Number of threads to use in the pipeline
   *
   * @type {?number}
   */
  numThreads = null;

  /**
   * Execution priority
   *
   * Defines the priority of the task
   *
   * @type {ExecutionPriority}
   */
  executionPriority = null;

  /**
   * Create a PipelineOptions instance.
   *
   * @param {object} options - The options for the pipeline. Must include mandatory fields.
   */
  constructor(options) {
    this.updateOptions(options);
  }

  /**
   * Determines if the pipeline is mocked.
   *
   * It is made static to enable easier global overriding during unit tests and to allow the
   * check to be performed without requiring an instance of the class.
   *
   * @param {object} options - The options for the pipeline.
   */
  static isMocked(options) {
    return (
      PIPELINE_TEST_NAMES.includes(options.taskName) ||
      PIPELINE_TEST_NAMES.includes(options.modelId)
    );
  }

  /**
   * Private method to validate enum fields.
   *
   * @param {string} field - The field being validated (e.g., 'dtype', 'device', 'executionPriority').
   * @param {*} value - The value being checked against the enum.
   * @throws {Error} Throws an error if the value is not valid.
   * @private
   */
  #validateEnum(field, value) {
    const enums = {
      dtype: QuantizationLevel,
      device: InferenceDevice,
      executionPriority: ExecutionPriority,
      logLevel: LogLevel,
      modelHub: ModelHub,
    };
    // Check if the value is part of the enum or null
    if (!Object.values(enums[field]).includes(value)) {
      throw new PipelineOptionsValidationError(field, value);
    }
  }

  /**
   * Validates the taskName field, ensuring it contains only alphanumeric characters, underscores, and dashes.
   * Slashes are not allowed in the taskName.
   *
   * @param {string} field - The name of the field being validated (e.g., taskName).
   * @param {string} value - The value of the field to validate.
   * @throws {Error} Throws an error if the taskName contains invalid characters.
   * @private
   */
  #validateTaskName(field, value) {
    // Define a regular expression to verify taskName pattern (alphanumeric, underscores, and dashes, no slashes)
    const validTaskNamePattern = /^[a-zA-Z0-9_\-]+$/;

    // Check if the value matches the pattern
    if (!validTaskNamePattern.test(value)) {
      throw new PipelineOptionsValidationError(
        field,
        value,
        "Should contain only alphanumeric characters, underscores, or dashes."
      );
    }
  }

  /**
   * Validates a taskName or ID.
   *
   * The ID can optionally be in the form `organization/name`, where both `organization` and `name`
   * follow the `taskName` pattern (alphanumeric characters, underscores, and dashes).
   *
   * Throws an exception if the name or ID is invalid.
   *
   * @param {string} field - The name of the field being validated (e.g., taskName, engineId).
   * @param {string} value - The value of the field to validate.
   * @throws {PipelineOptionsValidationError} Throws a validation error if the ID is invalid.
   * @private
   */
  #validateId(field, value) {
    // Define a regular expression to match the optional organization and required name
    // `organization/` part is optional, and both parts should follow the taskName pattern.
    const validPattern = /^(?:[a-zA-Z0-9_\-\.]+\/)?[a-zA-Z0-9_\-\.]+$/;

    // Check if the value matches the pattern
    if (!validPattern.test(value)) {
      throw new PipelineOptionsValidationError(
        field,
        value,
        "Should follow the format 'organization/name' or 'name', where both parts contain only alphanumeric characters, underscores, dots or dashes."
      );
    }
  }

  /**
   * Generic method to validate an integer within a specified range.
   *
   * @param {string} field - The name of the field being validated.
   * @param {number} value - The integer value to validate.
   * @param {number} min - The minimum allowed value (inclusive).
   * @param {number} max - The maximum allowed value (inclusive).
   * @throws {Error} Throws an error if the value is not a valid integer within the range.
   * @private
   */
  #validateIntegerRange(field, value, min, max) {
    if (!Number.isInteger(value) || value < min || value > max) {
      throw new PipelineOptionsValidationError(
        field,
        value,
        `Should be an integer between ${min} and ${max}.`
      );
    }
  }

  /**
   * Validates the revision field.
   * The revision can be `main` or a version following a pattern like `v1.0.0`, `1.0.0-beta1`, `1.0.0.alpha2`, `1.0.0.rc1`, etc.
   *
   * @param {string} field - The name of the field being validated (e.g., modelRevision, tokenizerRevision).
   * @param {string} value - The value of the revision field to validate.
   * @throws {Error} Throws an error if the revision does not follow the expected pattern.
   * @private
   */
  #validateRevision(field, value) {
    // Regular expression to match `main` or a version like `v1`, `v1.0.0`, `1.0.0-alpha1`, `1.0.0.alpha2`, `1.0.0.rc1`, etc.
    const revisionPattern =
      /^v?(\d+(\.\d+){0,2})([-\.](alpha\d*|beta\d*|pre\d*|post\d*|rc\d*))?$|^main$/;

    // Check if the value matches the pattern
    if (!revisionPattern.test(value)) {
      throw new PipelineOptionsValidationError(
        field,
        value,
        `Should be 'main' or follow a versioning pattern like 'v1.0.0', '1.0.0-beta1', '1.0.0.alpha2', '1.0.0.rc1', etc.`
      );
    }
  }

  /**
   * Updates multiple options at once.
   *
   * @param {object} options - An object containing the options to update.
   * @throws {Error} Throws an error if an invalid option is provided.
   */
  updateOptions(options) {
    const allowedKeys = [
      "engineId",
      "featureId",
      "taskName",
      "modelHub",
      "modelHubRootUrl",
      "modelHubUrlTemplate",
      "timeoutMS",
      "modelId",
      "modelRevision",
      "tokenizerId",
      "tokenizerRevision",
      "processorId",
      "processorRevision",
      "logLevel",
      "runtimeFilename",
      "device",
      "dtype",
      "numThreads",
      "executionPriority",
      "useExternalDataFormat",
    ];

    if (options instanceof PipelineOptions) {
      options = options.getOptions();
    }

    let optionsKeys = Object.keys(options);

    allowedKeys.forEach(key => {
      // If options does not have the key we can ignore it.
      // We also ignore `null` values.
      if (!optionsKeys.includes(key) || options[key] == null) {
        return;
      }
      if (key === "featureId" && !FEATURES.includes(options[key])) {
        throw new PipelineOptionsValidationError(
          key,
          options[key],
          `Should be one of ${FEATURES.join(", ")}`
        );
      }
      // Validating values.
      if (["taskName", "engineId"].includes(key)) {
        this.#validateTaskName(key, options[key]);
      }

      if (["modelId", "tokenizerId", "processorId"].includes(key)) {
        this.#validateId(key, options[key]);
      }

      if (
        ["modelRevision", "tokenizerRevision", "processorRevision"].includes(
          key
        )
      ) {
        this.#validateRevision(key, options[key]);
      }

      if (
        [
          "modelHub",
          "dtype",
          "device",
          "executionPriority",
          "logLevel",
        ].includes(key)
      ) {
        this.#validateEnum(key, options[key]);
      }

      if (key === "numThreads") {
        this.#validateIntegerRange(key, options[key], 0, 100);
      }

      if (key === "timeoutMS") {
        this.#validateIntegerRange(key, options[key], -1, 36000000);
      }

      if (key === "modelHub") {
        ModelHub.apply(this, options[key]);
      }

      this[key] = options[key];
    });
  }

  /**
   * Returns an object containing all current options.
   *
   * @returns {object} An object with the current options.
   */
  getOptions() {
    return {
      engineId: this.engineId,
      featureId: this.featureId,
      taskName: this.taskName,
      modelHub: this.modelHub,
      modelHubRootUrl: this.modelHubRootUrl,
      modelHubUrlTemplate: this.modelHubUrlTemplate,
      timeoutMS: this.timeoutMS,
      modelId: this.modelId,
      modelRevision: this.modelRevision,
      tokenizerId: this.tokenizerId,
      tokenizerRevision: this.tokenizerRevision,
      processorId: this.processorId,
      processorRevision: this.processorRevision,
      logLevel: this.logLevel,
      runtimeFilename: this.runtimeFilename,
      device: this.device,
      dtype: this.dtype,
      numThreads: this.numThreads,
      executionPriority: this.executionPriority,
      useExternalDataFormat: this.useExternalDataFormat,
    };
  }

  /**
   * Updates the given configuration object with the options.
   *
   * @param {object} config - The configuration object to be updated.
   */
  applyToConfig(config) {
    const options = this.getOptions();
    Object.keys(options).forEach(key => {
      if (options[key] !== null) {
        config[key] = options[key];
      }
    });
  }

  /**
   * Checks if this PipelineOptions instance is equal to another.
   *
   * @param {PipelineOptions} other - The other PipelineOptions instance to compare with.
   * @returns {boolean} True if the instances are equal, false otherwise.
   */
  equals(other) {
    if (!(other instanceof PipelineOptions)) {
      return false;
    }
    const options = this.getOptions();
    const otherOptions = other.getOptions();

    const isEqual = (val1, val2) => {
      if (val1 === val2) {
        return true;
      }
      if (val1 == null || val2 == null) {
        return false;
      }
      if (typeof val1 !== "object" || typeof val2 !== "object") {
        return false;
      }
      const keys1 = Object.keys(val1);
      const keys2 = Object.keys(val2);
      if (keys1.length !== keys2.length) {
        return false;
      }
      return keys1.every(key => isEqual(val1[key], val2[key]));
    };

    return Object.keys(options).every(key =>
      isEqual(options[key], otherOptions[key])
    );
  }
}

/**
 * This class controls the life cycle of the engine process used both in the
 * Translations engine and the MLEngine component.
 */
export class EngineProcess {
  /**
   * Get a reference to all running "inference" processes.
   *
   * @returns {sequence<nsIDOMProcessParent>}
   */
  static #inferenceProcesses() {
    return ChromeUtils.getAllDOMProcesses().filter(
      p => p.remoteType == "inference"
    );
  }

  /**
   * See if all engines are terminated and the "inference" process has been shut
   * down. This is useful for testing.
   *
   * @returns {boolean}
   */
  static areAllEnginesTerminated() {
    return !EngineProcess.#inferenceProcesses().length;
  }

  /**
   * @returns {Promise<TranslationsEngineParent>}
   */
  static async getTranslationsEngineParent() {
    return EngineProcess.#getEngineActor({ actorName: "TranslationsEngine" });
  }

  /**
   * @returns {Promise<MLEngineParent>}
   */
  static async getMLEngineParent() {
    // the pref is off by default
    if (!Services.prefs.getBoolPref("browser.ml.enable")) {
      throw new Error("MLEngine is disabled. Check the browser.ml prefs.");
    }

    return EngineProcess.#getEngineActor({ actorName: "MLEngine" });
  }

  /**
   * @returns {Promise<JSProcessActorParent>}
   */
  static async #getEngineActor({ actorName }) {
    let keepAlive = await ChromeUtils.ensureHeadlessContentProcess(
      "inference",
      { preferUsed: true }
    );
    if (!keepAlive?.domProcess?.canSend) {
      return null;
    }

    try {
      const actor = keepAlive.domProcess.getActor(actorName);
      if (actor && !actor.processKeepAlive) {
        ChromeUtils.addProfilerMarker(
          "EngineProcess",
          {},
          `Setting ${actorName} "inference" process keep-alive`
        );
        actor.processKeepAlive = keepAlive;
        keepAlive = null;
      }
      return actor;
    } finally {
      if (keepAlive) {
        keepAlive.invalidateKeepAlive();
      }
    }
  }

  /**
   * Send the `ForceShutdown` message to the TranslationsEngine, terminating
   * running engines, and potentially leading to "inference" process shutdown.
   */
  static destroyTranslationsEngine() {
    return EngineProcess.#forceShutdownEngine({
      actorName: "TranslationsEngine",
    });
  }

  /**
   * Send the `ForceShutdown` message to the MLEngine, terminating running
   * queries, and potentially leading to "inference" process shutdown.
   */
  static destroyMLEngine() {
    return EngineProcess.#forceShutdownEngine({ actorName: "MLEngine" });
  }

  static #forceShutdownEngine({ actorName }) {
    return Promise.allSettled(
      EngineProcess.#inferenceProcesses().map(async process => {
        let actor = process.getExistingActor(actorName);
        if (actor) {
          await actor.forceShutdown();

          // The actor should have cleared its own KeepAlive.
          if (actor.processKeepAlive) {
            ChromeUtils.addProfilerMarker(
              "EngineProcess",
              {},
              `Force-dropping ${actorName} "inference" process keep-alive`
            );
            actor.processKeepAlive.invalidateKeepAlive();
            actor.processKeepAlive = null;
          }
        }
      })
    );
  }
}

/**
 * Creates a new ML engine instance with the provided options.
 *
 * @param {object} options - Configuration options for the ML engine.
 * @param {?function(ProgressAndStatusCallbackParams):void} notificationsCallback A function to call to indicate notifications.
 * @returns {Promise<MLEngine>} - A promise that resolves to the ML engine instance.
 */
export async function createEngine(options, notificationsCallback = null) {
  const pipelineOptions = new PipelineOptions(options);
  const engineParent = await EngineProcess.getMLEngineParent();
  return engineParent.getEngine(pipelineOptions, notificationsCallback);
}
