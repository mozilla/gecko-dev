/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsDocument:
    "chrome://global/content/translations/translations-document.sys.mjs",
  LRUCache:
    "chrome://global/content/translations/translations-document.sys.mjs",
  LanguageDetector:
    "resource://gre/modules/translations/LanguageDetector.sys.mjs",
});

/**
 * This file is extremely sensitive to memory size and performance!
 */
export class TranslationsChild extends JSWindowActorChild {
  /**
   * @type {TranslationsDocument | null}
   */
  #translatedDoc = null;

  /**
   * This cache is shared across TranslationsChild instances. This means
   * that it will be shared across multiple page loads in the same origin.
   *
   * @type {LRUCache | null}
   */
  static #translationsCache = null;

  /**
   * Set to true when this actor is destroyed.
   *
   * It is important to check this variable before sending any asynchronous
   * messages to the parent actor. If this actor has been destroyed, then
   * sending a message will result in an error.
   *
   * @see {TranslationsChild.didDestroy}
   *
   * @type {boolean}
   */
  #isDestroyed = false;

  handleEvent(event) {
    if (this.#isDestroyed) {
      return;
    }

    switch (event.type) {
      case "DOMContentLoaded":
        this.sendAsyncMessage("Translations:ReportLangTags", {
          documentElementLang: this.document.documentElement.lang,
        });
        break;
    }
  }

  didDestroy() {
    this.#isDestroyed = true;
    this.#translatedDoc?.destroy();
    this.#translatedDoc = null;
  }

  /**
   * Returns true if the TranslationsDocument has any callbacks pending to run on
   * the event loop, otherwise false.
   *
   * @returns {boolean}
   */
  hasPendingCallbackOnEventLoop() {
    if (!this.#translatedDoc) {
      // Full-Page Translations has not been requested yet, so there is no callback.
      return false;
    }

    return this.#translatedDoc.hasPendingCallbackOnEventLoop();
  }

  /**
   * Returns true if the TranslationsDocument has any pending translation requests, otherwise false.
   *
   * Having no pending request does NOT mean that the entire page is translated, nor does it mean
   * that more requests won't come in via mutations or intersection observations. It simply means
   * that there are no pending requests at this exact moment.
   *
   * @returns {boolean}
   */
  hasPendingTranslationRequests() {
    if (!this.#translatedDoc) {
      // Full-Page Translations has not been requested yet, so there are no requests.
      return false;
    }

    return this.#translatedDoc.hasPendingTranslationRequests();
  }

  /**
   * Returns true if the TranslationsDocument is observing any element for content translation, otherwise false.
   *
   * Having no observed elements means that at the current point in time, until any further mutations occur,
   * every content translation request has been fulfilled.
   *
   * @returns {boolean}
   */
  isObservingAnyElementForContentIntersection() {
    if (!this.#translatedDoc) {
      // Full-Page Translations has not been requested yet, so we are not observing.
      return false;
    }

    return this.#translatedDoc.isObservingAnyElementForContentIntersection();
  }

  /**
   * Returns true if the TranslationsDocument is observing any element for attribute translation, otherwise false.
   *
   * Having no observed elements means that at the current point in time, until any further mutations occur,
   * every attribute translation request has been fulfilled.
   *
   * @returns {boolean}
   */
  isObservingAnyElementForAttributeIntersection() {
    if (!this.#translatedDoc) {
      // Full-Page Translations has not been requested yet, so we are not observing.
      return false;
    }

    return this.#translatedDoc.isObservingAnyElementForAttributeIntersection();
  }

  addProfilerMarker(message, startTime) {
    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      {
        innerWindowId: this.contentWindow?.windowGlobalChild.innerWindowId,
        startTime,
      },
      message
    );
  }

  async receiveMessage({ name, data }) {
    if (this.#isDestroyed) {
      return undefined;
    }

    switch (name) {
      case "Translations:FindBarOpen": {
        this.#translatedDoc?.enterContentEagerTranslationsMode();
        return undefined;
      }
      case "Translations:FindBarClose": {
        this.#translatedDoc?.enterLazyTranslationsMode();
        return undefined;
      }
      case "Translations:TranslatePage": {
        if (this.#translatedDoc?.engineStatus === "error") {
          this.#translatedDoc.destroy();
          this.#translatedDoc = null;
        }

        if (this.#translatedDoc) {
          console.error("This page was already translated.");
          return undefined;
        }

        const { isFindBarOpen, languagePair, port } = data;

        if (
          !TranslationsChild.#translationsCache ||
          !TranslationsChild.#translationsCache.matches(languagePair)
        ) {
          TranslationsChild.#translationsCache = new lazy.LRUCache(
            languagePair
          );
        }

        this.#translatedDoc = new lazy.TranslationsDocument(
          this.document,
          languagePair.sourceLanguage,
          languagePair.targetLanguage,
          this.contentWindow.windowGlobalChild.innerWindowId,
          port,
          () => this.sendAsyncMessage("Translations:RequestPort"),
          () => this.sendAsyncMessage("Translations:ReportFirstVisibleChange"),
          TranslationsChild.#translationsCache,
          isFindBarOpen
        );

        return undefined;
      }
      case "Translations:GetDocumentElementLang": {
        return this.document.documentElement.lang;
      }
      case "Translations:IdentifyLanguage": {
        // Wait for idle callback as the page will be more settled if it has
        // dynamic content, like on a React app.
        if (this.contentWindow) {
          await new Promise(resolve => {
            this.contentWindow.requestIdleCallback(resolve);
          });
        }

        if (this.#isDestroyed) {
          return undefined;
        }

        const startTime = Cu.now();
        const detectionResult =
          await lazy.LanguageDetector.detectLanguageFromDocument(this.document);

        if (this.#isDestroyed) {
          return undefined;
        }

        this.addProfilerMarker(
          `Detect language from document: ${detectionResult.language}`,
          startTime
        );
        return detectionResult;
      }
      case "Translations:AcquirePort": {
        this.addProfilerMarker("Acquired a port, resuming translations");
        this.#translatedDoc.acquirePort(data.port);
        return undefined;
      }
      default:
        throw new Error("Unknown message.", name);
    }
  }
}
