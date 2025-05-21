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

  get translatedDoc() {
    return this.#translatedDoc;
  }

  /**
   * This cache is shared across TranslationsChild instances. This means
   * that it will be shared across multiple page loads in the same origin.
   *
   * @type {LRUCache | null}
   */
  static #translationsCache = null;

  #isDestroyed = false;

  handleEvent(event) {
    if (this.#isDestroyed) {
      return;
    }

    if (event.type === "DOMContentLoaded") {
      this.sendAsyncMessage("Translations:ReportLangTags", {
        documentElementLang: this.document.documentElement.lang,
      });
    }
  }

  didDestroy() {
    this.#isDestroyed = true;
    this.#translatedDoc?.destroy();
    this.#translatedDoc = null;
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
