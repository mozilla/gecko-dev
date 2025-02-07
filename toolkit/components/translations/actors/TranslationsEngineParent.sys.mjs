/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsParent: "resource://gre/actors/TranslationsParent.sys.mjs",
  TranslationsTelemetry:
    "chrome://global/content/translations/TranslationsTelemetry.sys.mjs",
});

/**
 * @typedef {import("../translations").LanguagePair} LanguagePair
 */

/**
 * The translations engine is in its own content process. This actor handles the
 * marshalling of the data such as the engine payload and port passing.
 */
export class TranslationsEngineParent extends JSProcessActorParent {
  /**
   * Keep track of the live actors by InnerWindowID.
   *
   * @type {Map<InnerWindowID, TranslationsParent | AboutTranslationsParent>}
   */
  #translationsParents = new Map();

  /**
   * Set by EngineProcess when creating the TranslationsEngineParent.
   * Keeps the "inference" process alive until it is cleared.
   *
   * NOTE: Invalidating this keepAlive does not guarantee that the process will
   * exit, and this actor may be re-used if it does not (e.g. because the
   * inference process was kept alive by MLEngine).
   *
   * @type {nsIContentParentKeepAlive | null}
   */
  processKeepAlive = null;

  async receiveMessage({ name, data }) {
    switch (name) {
      case "TranslationsEngine:RequestEnginePayload": {
        const { languagePair } = data;
        const payloadPromise =
          lazy.TranslationsParent.getTranslationsEnginePayload(languagePair);
        payloadPromise.catch(error => {
          lazy.TranslationsParent.telemetry().onError(String(error));
        });
        return payloadPromise;
      }
      case "TranslationsEngine:ReportEnginePerformance": {
        const {
          sourceLanguage,
          targetLanguage,
          totalInferenceSeconds,
          totalTranslatedWords,
          totalCompletedRequests,
        } = data;
        lazy.TranslationsTelemetry.onReportEnginePerformance({
          sourceLanguage,
          targetLanguage,
          totalInferenceSeconds,
          totalTranslatedWords,
          totalCompletedRequests,
        });
        return undefined;
      }
      case "TranslationsEngine:ReportEngineStatus": {
        const { innerWindowId, status } = data;
        const translationsParent = this.#translationsParents.get(innerWindowId);

        // about:translations will not have a TranslationsParent associated with
        // this call.
        if (translationsParent) {
          switch (status) {
            case "ready":
              translationsParent.languageState.isEngineReady = true;
              break;
            case "error":
              translationsParent.languageState.error = "engine-load-failure";
              break;
            default:
              throw new Error("Unknown engine status: " + status);
          }
        }
        return undefined;
      }
      case "TranslationsEngine:DestroyEngineProcess":
        if (this.processKeepAlive) {
          ChromeUtils.addProfilerMarker(
            "EngineProcess",
            {},
            `Dropping TranslationsEngine "inference" process keep-alive`
          );
          this.processKeepAlive.invalidateKeepAlive();
          this.processKeepAlive = null;
        }
        return undefined;
      default:
        return undefined;
    }
  }

  /**
   * @param {LanguagePair} languagePair
   * @param {MessagePort} port
   * @param {TranslationsParent} [translationsParent]
   */
  startTranslation(languagePair, port, translationsParent) {
    const innerWindowId = translationsParent?.innerWindowId;
    if (translationsParent) {
      this.#translationsParents.set(innerWindowId, translationsParent);
    }
    if (this.#isDestroyed) {
      throw new Error("The translation engine process was already destroyed.");
    }
    const transferables = [port];
    this.sendAsyncMessage(
      "TranslationsEngine:StartTranslation",
      {
        languagePair,
        innerWindowId,
        port,
      },
      transferables
    );
  }

  /**
   * Remove all the translations that are currently queued, and remove
   * the communication port.
   *
   * @param {number} innerWindowId
   */
  discardTranslations(innerWindowId) {
    this.#translationsParents.delete(innerWindowId);
    this.sendAsyncMessage("TranslationsEngine:DiscardTranslations", {
      innerWindowId,
    });
  }

  /**
   * Manually shut down the engines, typically for testing purposes.
   */
  forceShutdown() {
    return this.sendQuery("TranslationsEngine:ForceShutdown");
  }

  #isDestroyed = false;

  didDestroy() {
    this.#isDestroyed = true;
  }
}
