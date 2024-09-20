/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(
  lazy,
  {
    HiddenFrame: "resource://gre/modules/HiddenFrame.sys.mjs",
  },
  { global: "current" }
);

/**
 * @typedef {import("../actors/MLEngineParent.sys.mjs").MLEngineParent} MLEngineParent
 * @typedef {import("../content/Utils.sys.mjs").ProgressAndStatusCallbackParams} ProgressAndStatusCallbackParams
 */

/**
 * @typedef {import("../../translations/actors/TranslationsEngineParent.sys.mjs").TranslationsEngineParent} TranslationsEngineParent
 */

/**
 * This class encapsulates the options for a pipeline process.
 */
export class PipelineOptions {
  /**
   * The identifier for the engine to be used by the pipeline.
   *
   * @type {?string}
   */
  engineId = "default-engine";

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
   * @type {?string}
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
   * @type {"gpu" | "wasm"}
   */
  device = "wasm";

  /**
   * Quantization level
   *
   * - name : description (onnx file suffix)
   * - 'fp32': Full precision 32-bit floating point (`''`)
   * - 'fp16': Half precision 16-bit floating point (`'_fp16'`)
   * - 'q8': Quantized 8-bit (`'_quantized'`)
   * - 'int8': Integer 8-bit quantization (`'_int8'`)
   * - 'uint8': Unsigned integer 8-bit quantization (`'_uint8'`)
   * - 'q4': Quantized 4-bit (`'_q4'`)
   * - 'bnb4': Binary/Boolean 4-bit quantization (`'_bnb4'`)
   * - 'q4f16': 16-bit floating point model with 4-bit block weight quantization (`'_q4f16'`)
   *
   * @type {"fp32" | "fp16" | "q8" | "int8" | "uint8" | "q4" | "bnb4" | "q4f16"}
   */
  dtype = "fp32";

  /**
   * Create a PipelineOptions instance.
   *
   * @param {object} options - The options for the pipeline. Must include mandatory fields.
   */
  constructor(options) {
    this.updateOptions(options);
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
      "taskName",
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
      this[key] = options[key];
    });
  }

  /**
   * Returns an object containing all current options.

   * @returns {object} An object with the current options.
   */
  getOptions() {
    return {
      engineId: this.engineId,
      taskName: this.taskName,
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
   * @type {Promise<{ hiddenFrame: HiddenFrame, actor: TranslationsEngineParent }> | null}
   */

  /** @type {Promise<HiddenFrame> | null} */
  static #hiddenFrame = null;
  /** @type {Promise<TranslationsEngineParent> | null} */
  static translationsEngineParent = null;
  /** @type {Promise<MLEngineParent> | null} */
  static mlEngineParent = null;

  /** @type {((actor: TranslationsEngineParent) => void) | null} */
  resolveTranslationsEngineParent = null;

  /** @type {((actor: MLEngineParent) => void) | null} */
  resolveMLEngineParent = null;

  /**
   * See if all engines are terminated. This is useful for testing.
   *
   * @returns {boolean}
   */
  static areAllEnginesTerminated() {
    return (
      !EngineProcess.#hiddenFrame &&
      !EngineProcess.translationsEngineParent &&
      !EngineProcess.mlEngineParent
    );
  }

  /**
   * @returns {Promise<TranslationsEngineParent>}
   */
  static async getTranslationsEngineParent() {
    if (!this.translationsEngineParent) {
      this.translationsEngineParent = this.#attachBrowser({
        id: "translations-engine-browser",
        url: "chrome://global/content/translations/translations-engine.html",
        resolverName: "resolveTranslationsEngineParent",
      });
    }
    return this.translationsEngineParent;
  }

  /**
   * @returns {Promise<MLEngineParent>}
   */
  static async getMLEngineParent() {
    // the pref is off by default
    if (!Services.prefs.getBoolPref("browser.ml.enable")) {
      throw new Error("MLEngine is disabled. Check the browser.ml prefs.");
    }

    if (!this.mlEngineParent) {
      this.mlEngineParent = this.#attachBrowser({
        id: "ml-engine-browser",
        url: "chrome://global/content/ml/MLEngine.html",
        resolverName: "resolveMLEngineParent",
      });
    }
    return this.mlEngineParent;
  }

  /**
   * @param {object} config
   * @param {string} config.url
   * @param {string} config.id
   * @param {string} config.resolverName
   * @returns {Promise<TranslationsEngineParent>}
   */
  static async #attachBrowser({ url, id, resolverName }) {
    const hiddenFrame = await this.#getHiddenFrame();
    const chromeWindow = await hiddenFrame.get();
    const doc = chromeWindow.document;

    if (doc.getElementById(id)) {
      throw new Error(
        "Attempting to append the translations-engine.html <browser> when one " +
          "already exists."
      );
    }

    const browser = doc.createXULElement("browser");
    browser.setAttribute("id", id);
    browser.setAttribute("remote", "true");
    browser.setAttribute("remoteType", "inference");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("src", url);

    ChromeUtils.addProfilerMarker(
      "EngineProcess",
      {},
      `Creating the "${id}" process`
    );
    doc.documentElement.appendChild(browser);

    const { promise, resolve } = Promise.withResolvers();

    // The engine parents must resolve themselves when they are ready.
    this[resolverName] = resolve;

    return promise;
  }

  /**
   * @returns {HiddenFrame}
   */
  static async #getHiddenFrame() {
    if (!EngineProcess.#hiddenFrame) {
      EngineProcess.#hiddenFrame = new lazy.HiddenFrame();
    }
    return EngineProcess.#hiddenFrame;
  }

  /**
   * Destroy the translations engine, and remove the hidden frame if no other
   * engines exist.
   */
  static destroyTranslationsEngine() {
    return this.#destroyEngine({
      id: "translations-engine-browser",
      keyName: "translationsEngineParent",
    });
  }

  /**
   * Destroy the ML engine, and remove the hidden frame if no other engines exist.
   */
  static destroyMLEngine() {
    return this.#destroyEngine({
      id: "ml-engine-browser",
      keyName: "mlEngineParent",
    });
  }

  /**
   * Destroy the specified engine and maybe the entire hidden frame as well if no engines
   * are remaining.
   */
  static async #destroyEngine({ id, keyName }) {
    ChromeUtils.addProfilerMarker(
      "EngineProcess",
      {},
      `Destroying the "${id}" engine`
    );

    let actorShutdown = this.forceActorShutdown(id, keyName);

    this[keyName] = null;

    const hiddenFrame = EngineProcess.#hiddenFrame;
    if (hiddenFrame && !this.translationsEngineParent && !this.mlEngineParent) {
      EngineProcess.#hiddenFrame = null;

      // Both actors are destroyed, also destroy the hidden frame.
      actorShutdown = actorShutdown.then(() => {
        // Double check a race condition that no new actors have been created during
        // shutdown.
        if (this.translationsEngineParent && this.mlEngineParent) {
          return;
        }
        if (!hiddenFrame) {
          return;
        }
        hiddenFrame.destroy();
        ChromeUtils.addProfilerMarker(
          "EngineProcess",
          {},
          `Removing the hidden frame`
        );
      });
    }

    // Infallibly resolve this promise even if there are errors.
    try {
      await actorShutdown;
    } catch (error) {
      console.error(error);
    }
  }

  /**
   * Shut down an actor and remove its <browser> element.
   *
   * @param {string} id
   * @param {string} keyName
   */
  static async forceActorShutdown(id, keyName) {
    const actorPromise = this[keyName];
    if (!actorPromise) {
      return;
    }

    let actor;
    try {
      actor = await actorPromise;
    } catch {
      // The actor failed to initialize, so it doesn't need to be shut down.
      return;
    }

    // Shut down the actor.
    try {
      await actor.forceShutdown();
    } catch (error) {
      console.error("Failed to shut down the actor " + id, error);
      return;
    }

    if (!EngineProcess.#hiddenFrame) {
      // The hidden frame was already removed.
      return;
    }

    // Remove the <brower> element.
    const chromeWindow = EngineProcess.#hiddenFrame.getWindow();
    const doc = chromeWindow.document;
    const element = doc.getElementById(id);
    if (!element) {
      console.error("Could not find the <browser> element for " + id);
      return;
    }
    element.remove();
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
