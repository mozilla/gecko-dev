/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  handleActorMessage:
    "chrome://global/content/translations/translations-engine.sys.mjs",
});

/**
 * The engine child is responsible for exposing privileged code to the un-privileged
 * space the engine runs in.
 */
export class TranslationsEngineChild extends JSProcessActorChild {
  /**
   * The resolve function for the Promise returned by the
   * "TranslationsEngine:ForceShutdown" message.
   *
   * @type {null | () => {}}
   */
  #resolveForceShutdown = null;

  // eslint-disable-next-line consistent-return
  async receiveMessage({ name, data }) {
    switch (name) {
      case "TranslationsEngine:StartTranslation": {
        const { languagePair, innerWindowId, port } = data;
        const message = {
          type: "StartTranslation",
          languagePair,
          innerWindowId,
          port,
        };
        lazy.handleActorMessage(message);
        break;
      }
      case "TranslationsEngine:DiscardTranslations": {
        const { innerWindowId } = data;
        lazy.handleActorMessage({
          type: "DiscardTranslations",
          innerWindowId,
        });
        break;
      }
      case "TranslationsEngine:ForceShutdown": {
        lazy.handleActorMessage({
          type: "ForceShutdown",
        });
        return new Promise(resolve => {
          this.#resolveForceShutdown = resolve;
        });
      }
      default:
        console.error("Unknown message received", name);
    }
  }

  /**
   * @param {object} options
   * @param {number?} options.startTime
   * @param {string} options.message
   * @param {number} options.innerWindowId
   */
  TE_addProfilerMarker({ startTime, message, innerWindowId }) {
    ChromeUtils.addProfilerMarker(
      "TranslationsEngine",
      { startTime, innerWindowId },
      message
    );
  }

  /**
   * Pass the message from content that the engines were shut down.
   */
  TE_resolveForceShutdown() {
    this.#resolveForceShutdown();
    this.#resolveForceShutdown = null;
  }

  /**
   * @returns {string}
   */
  TE_getLogLevel() {
    return Services.prefs.getCharPref("browser.translations.logLevel");
  }

  /**
   * Log messages if "browser.translations.logLevel" is set to "All".
   *
   * @param {...any} args
   */
  TE_log(...args) {
    lazy.console.log(...args);
  }

  /**
   * Report an error to the console.
   *
   * @param {...any} args
   */
  TE_logError(...args) {
    lazy.console.error(...args);
  }

  /**
   * Reports translation engine performance data to telemetry.
   *
   * @param {object} data
   * @param {string} data.sourceLanguage - The BCP-47 language tag of the source text.
   * @param {string} data.targetLanguage - The BCP-47 language tag of the target text.
   * @param {number} data.totalInferenceSeconds - Total total seconds spent in active translation inference.
   * @param {number} data.totalTranslatedWords - Total total count of words that were translated.
   * @param {number} data.totalCompletedRequests - Total total count of completed translation requests.
   */
  TE_reportEnginePerformance({
    sourceLanguage,
    targetLanguage,
    totalInferenceSeconds,
    totalTranslatedWords,
    totalCompletedRequests,
  }) {
    this.sendAsyncMessage("TranslationsEngine:ReportEnginePerformance", {
      sourceLanguage,
      targetLanguage,
      totalInferenceSeconds,
      totalTranslatedWords,
      totalCompletedRequests,
    });
  }

  /**
   * @param {LanguagePair} languagePair
   */
  TE_requestEnginePayload(languagePair) {
    return this.sendQuery("TranslationsEngine:RequestEnginePayload", {
      languagePair,
    });
  }

  /**
   * @param {number} innerWindowId
   * @param {"ready" | "error"} status
   */
  TE_reportEngineStatus(innerWindowId, status) {
    this.sendAsyncMessage("TranslationsEngine:ReportEngineStatus", {
      innerWindowId,
      status,
    });
  }

  /**
   * No engines are still alive, signal that the process can be destroyed.
   */
  TE_destroyEngineProcess() {
    this.sendAsyncMessage("TranslationsEngine:DestroyEngineProcess");
  }
}
