/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionCommon } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionCommon.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  PipelineOptions: "chrome://global/content/ml/EngineProcess.sys.mjs",
  ModelHub: "chrome://global/content/ml/ModelHub.sys.mjs",
});

const PREF_EXTENSIONS_ML_ENABLED = "extensions.ml.enabled";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "extensionInferenceEnabled",
  PREF_EXTENSIONS_ML_ENABLED,
  false
);

const PREF_BROWSER_ML_ENABLE = "browser.ml.enable";
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "browserInferenceEnable",
  PREF_BROWSER_ML_ENABLE,
  false
);

function ensureInferenceEnabled() {
  if (extensionInferenceEnabled && browserInferenceEnable) {
    return;
  }
  throw new ExtensionError(
    `Trial ML is only available when "${PREF_EXTENSIONS_ML_ENABLED}" and ${PREF_BROWSER_ML_ENABLE}" preferences are set to true.`
  );
}

var { ExtensionError } = ExtensionUtils;

// We could have that list in remote settings as well
const SUPPORTED_TASKS = [
  "text-classification",
  "token-classification",
  "question-answering",
  "fill-mask",
  "summarization",
  "translation",
  "text2text-generation",
  "text-generation",
  "zero-shot-classification",
  "image-to-text",
  "image-classification",
  "image-segmentation",
  "zero-shot-image-classification",
  "object-detection",
  "zero-shot-object-detection",
  "document-question-answering",
  "image-to-image",
  "depth-estimation",
  "feature-extraction",
  "image-feature-extraction",
];

const ENGINE_EVENT = "MLEngine:progress";
const modelHub = new ModelHub();

/**
 * TrialML class provides a machine learning pipeline for extensions.
 * It handles the creation and running of engines for different machine learning tasks.
 */
class TrialML extends ExtensionAPI {
  #pipelineId = null;
  #engine = null;
  #pipelineOptions = null;

  /**
   * Constructor for TrialML
   *
   * @param {object} extension - The extension object, one instance per web extension.
   */
  constructor(extension) {
    super(extension);
    this.#pipelineId = TrialML.getPipelineId(extension.id);
    this.#engine = null;
  }

  /**
   * Converts an extension id to a pipeline id
   */
  static getPipelineId(extensionId) {
    // makeWidgetId() replaces all illegal characters with "_"
    // so an id like {XXX-XXX-XXX} becomes _XXX_XXX_XXX_
    // which is then converted to ML-ENGINE-XXX-XXX-XXX
    return `ML-ENGINE-${ExtensionCommon.makeWidgetId(extensionId)
      .replace(/^_|_$/g, "")
      .replace(/_/g, "-")}`;
  }

  /**
   * Private method to create an engine for the ML pipeline.
   *
   * @param {object} options - The options for engine creation.
   * @throws {ExtensionError} If the extension has already shut down or if the task is unsupported.
   */
  async #createEngine(options) {
    if (this.#engine) {
      throw new ExtensionError("Engine already created");
    }
    options.engineId = this.#pipelineId;
    const { extension } = this;
    if (!this.extension || this.extension.hasShutdown) {
      throw new ExtensionError("Extension has already shutdown");
    }
    this.#engine = await createEngine(options, progressData => {
      extension.emit(ENGINE_EVENT, progressData);
    });
  }

  /**
   * Private method to run the engine.
   *
   * @param {object} runOptions - The options for running the engine.
   * @returns {Promise} The result of the engine run.
   */
  #runEngine(runOptions) {
    return this.#engine.run(runOptions);
  }

  /**
   * Called on extension uninstall
   */
  static async onUninstall(extensionId) {
    await modelHub.deleteFilesByEngine(TrialML.getPipelineId(extensionId));
    return true;
  }

  /**
   * Public method to expose the API for creating and running ML pipelines.
   *
   * @param {object} context - The context of the API.
   * @returns {object} API for creating and running ML pipelines, and listening for progress events.
   */
  getAPI(context) {
    ensureInferenceEnabled();

    return {
      trial: {
        ml: {
          /**
           * Creates an ML Engine based on the provided task name and options.
           *
           * @param {object} request - The request object containing the task name and other options.
           * @throws {ExtensionError} If the task is unsupported.
           * @returns {Promise} The result of the pipeline creation.
           */
          createEngine: async request => {
            if (!SUPPORTED_TASKS.includes(request.taskName)) {
              throw new ExtensionError(`Unsupported task ${request.taskName}`);
            }
            // The request is converted in a PipelineOptions object.
            // That constructor checks for any value issue and will error out.
            // We can catch it here to provide nice error messages
            try {
              this.#pipelineOptions = new PipelineOptions(request);
              await this.#createEngine(this.#pipelineOptions);
            } catch (error) {
              throw new ExtensionError(error.message);
            }
          },

          /**
           * Runs the created ML Engine with the given options.
           *
           * @param {object} request - The request object containing arguments and other options.
           * @returns {Promise} The result of the pipeline run.
           */
          runEngine: async request => {
            if (this.#engine?.engineStatus === "closed") {
              // Engine closed for inactivity, re-create it with saved options.
              try {
                this.#engine = null;
                await this.#createEngine(this.#pipelineOptions);
              } catch (error) {
                throw new ExtensionError(error.message);
              }
            }
            const runOptions = {
              args: request.args,
              options: request.options || {},
            };
            return this.#runEngine(runOptions);
          },

          /**
           * Deletes all the models downloaded for this extension.
           */
          deleteCachedModels: async () => {
            await modelHub.deleteFilesByEngine(this.#pipelineId);
            return true;
          },

          /**
           * Registers an event listener for progress events during pipeline execution.
           *
           * @type {EventManager}
           */
          onProgress: new EventManager({
            context,
            name: "trial.ml.onProgress",
            register: fire => {
              const callback = (_evtName, progressData) => {
                fire.async(progressData);
              };
              this.extension.on(ENGINE_EVENT, callback);
              return () => {
                this.extension.off(ENGINE_EVENT, callback);
              };
            },
          }).api(),
        },
      },
    };
  }
}

this.trial_ml = TrialML;
