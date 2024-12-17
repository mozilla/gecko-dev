/* Copyright 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { PdfJsTelemetry } from "resource://pdf.js/PdfJsTelemetry.sys.mjs";
import { playSound } from "resource://gre/modules/FinderSound.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  EngineProcess: "chrome://global/content/ml/EngineProcess.sys.mjs",
  IndexedDBCache: "chrome://global/content/ml/ModelHub.sys.mjs",
  MultiProgressAggregator: "chrome://global/content/ml/Utils.sys.mjs",
  Progress: "chrome://global/content/ml/Utils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SetClipboardSearchString: "resource://gre/modules/Finder.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const IMAGE_TO_TEXT_TASK = "moz-image-to-text";
const ML_ENGINE_ID = "pdfjs";
const ML_ENGINE_MAX_TIMEOUT = 60000;

var Svc = {};
XPCOMUtils.defineLazyServiceGetter(
  Svc,
  "mime",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "matchesCountLimit",
  "accessibility.typeaheadfind.matchesCountLimit"
);

let gFindTypes = [
  "find",
  "findagain",
  "findhighlightallchange",
  "findcasesensitivitychange",
  "findbarclose",
  "finddiacriticmatchingchange",
];

export class PdfjsParent extends JSWindowActorParent {
  #mutablePreferences = new Set([
    "enableGuessAltText",
    "enableAltTextModelDownload",
    "enableNewAltTextWhenAddingImage",
  ]);

  constructor() {
    super();
    this._boundToFindbar = null;
    this._findFailedString = null;
    this._lastNotFoundStringLength = 0;

    this._updatedPreference();
  }

  didDestroy() {
    this._removeEventListener();
  }

  receiveMessage(aMsg) {
    switch (aMsg.name) {
      case "PDFJS:Parent:updateControlState":
        return this._updateControlState(aMsg);
      case "PDFJS:Parent:updateMatchesCount":
        return this._updateMatchesCount(aMsg);
      case "PDFJS:Parent:addEventListener":
        return this._addEventListener();
      case "PDFJS:Parent:saveURL":
        return this._saveURL(aMsg);
      case "PDFJS:Parent:recordExposure":
        return this._recordExposure();
      case "PDFJS:Parent:reportTelemetry":
        return this._reportTelemetry(aMsg);
      case "PDFJS:Parent:mlGuess":
        return this._mlGuess(aMsg);
      case "PDFJS:Parent:setPreferences":
        return this._setPreferences(aMsg);
      case "PDFJS:Parent:loadAIEngine":
        return this._loadAIEngine(aMsg);
      case "PDFJS:Parent:mlDelete":
        return this._mlDelete(aMsg);
      case "PDFJS:Parent:updatedPreference":
        return this._updatedPreference(aMsg);
    }
    return undefined;
  }

  /*
   * Internal
   */

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  _updatedPreference() {
    PdfJsTelemetry.report({
      type: "editing",
      data: {
        type: "stamp",
        action: "pdfjs.image.alt_text_edit",
        data: {
          ask_to_edit:
            Services.prefs.getBoolPref("pdfjs.enableAltText", false) &&
            Services.prefs.getBoolPref(
              "pdfjs.enableNewAltTextWhenAddingImage",
              false
            ),
          ai_generation:
            Services.prefs.getBoolPref("pdfjs.enableAltText", false) &&
            Services.prefs.getBoolPref("pdfjs.enableGuessAltText", false) &&
            Services.prefs.getBoolPref(
              "pdfjs.enableAltTextModelDownload",
              false
            ) &&
            Services.prefs.getBoolPref("browser.ml.enable", false),
        },
      },
    });
  }

  _setPreferences({ data }) {
    if (!data || typeof data !== "object") {
      return;
    }
    const branch = Services.prefs.getBranch("pdfjs.");
    for (const [key, value] of Object.entries(data)) {
      if (!this.#mutablePreferences.has(key)) {
        continue;
      }
      switch (branch.getPrefType(key)) {
        case Services.prefs.PREF_STRING:
          if (typeof value === "string") {
            branch.setStringPref(key, value);
          }
          break;
        case Services.prefs.PREF_INT:
          if (Number.isInteger(value)) {
            branch.setIntPref(key, value);
          }
          break;
        case Services.prefs.PREF_BOOL:
          if (typeof value === "boolean") {
            branch.setBoolPref(key, value);
          }
          break;
      }
    }
  }

  _recordExposure() {
    lazy.NimbusFeatures.pdfjs.recordExposureEvent({ once: true });
  }

  _reportTelemetry({ data }) {
    PdfJsTelemetry.report(data);
  }

  async _mlGuess({ data: { service, request } }) {
    if (service !== IMAGE_TO_TEXT_TASK) {
      return null;
    }
    try {
      const now = Cu.now();

      let response;
      if (Cu.isInAutomation) {
        response = { output: "In Automation" };
      } else {
        const engine = await this.#createAIEngine(service, null);
        response = await engine.run(request);
      }

      const time = Cu.now() - now;
      const length = response?.output.length ?? 0;
      PdfJsTelemetry.report({
        type: "editing",
        data: {
          type: "stamp",
          action: "pdfjs.image.alt_text.model_result",
          data: { time, length },
        },
      });
      return response;
    } catch (e) {
      console.error("Failed to run AI engine", e);
      return { error: true };
    }
  }

  async _loadAIEngine({ data: { service, listenToProgress } }) {
    if (service !== IMAGE_TO_TEXT_TASK) {
      throw new Error("Invalid service");
    }

    if (Cu.isInAutomation) {
      PdfJsTelemetry.report({
        type: "editing",
        data: {
          type: "stamp",
          action: "pdfjs.image.alt_text.model_download_start",
        },
      });
      PdfJsTelemetry.report({
        type: "editing",
        data: {
          type: "stamp",
          action: "pdfjs.image.alt_text.model_download_complete",
        },
      });
      return true;
    }

    let hasDownloadStarted = false;
    const self = this;
    const timeoutCallback = () => {
      lazy.clearTimeout(timeoutId);
      timeoutId = null;
      if (hasDownloadStarted) {
        PdfJsTelemetry.report({
          type: "editing",
          data: {
            type: "stamp",
            action: "pdfjs.image.alt_text.model_download_error",
          },
        });
      }
      if (!listenToProgress) {
        return;
      }
      self.sendAsyncMessage("PDFJS:Child:handleEvent", {
        type: "loadAIEngineProgress",
        detail: {
          service,
          ok: false,
          finished: true,
        },
      });
    };
    let timeoutId = lazy.setTimeout(timeoutCallback, ML_ENGINE_MAX_TIMEOUT);
    const aggregator = new lazy.MultiProgressAggregator({
      progressCallback({ ok, total, totalLoaded, statusText, type }) {
        if (timeoutId !== null) {
          lazy.clearTimeout(timeoutId);
          timeoutId = lazy.setTimeout(timeoutCallback, ML_ENGINE_MAX_TIMEOUT);
        } else {
          // The timeout has already fired, so we don't need to do anything.
          this.progressCallback = null;
          return;
        }
        if (
          !hasDownloadStarted &&
          type === lazy.Progress.ProgressType.DOWNLOAD
        ) {
          hasDownloadStarted = true;
          PdfJsTelemetry.report({
            type: "editing",
            data: {
              type: "stamp",
              action: "pdfjs.image.alt_text.model_download_start",
            },
          });
        }
        const finished = statusText === lazy.Progress.ProgressStatusText.DONE;
        if (listenToProgress) {
          self.sendAsyncMessage("PDFJS:Child:handleEvent", {
            type: "loadAIEngineProgress",
            detail: {
              service,
              ok,
              total,
              totalLoaded,
              finished,
            },
          });
        }
        if (finished) {
          if (
            hasDownloadStarted &&
            type === lazy.Progress.ProgressType.DOWNLOAD
          ) {
            PdfJsTelemetry.report({
              type: "editing",
              data: {
                type: "stamp",
                action: `pdfjs.image.alt_text.model_download_${
                  ok ? "complete" : "error"
                }`,
              },
            });
          }

          lazy.clearTimeout(timeoutId);
          // Once we're done, we can remove the progress callback.
          this.progressCallback = null;
        }
      },
      watchedTypes: [
        lazy.Progress.ProgressType.DOWNLOAD,
        lazy.Progress.ProgressType.LOAD_FROM_CACHE,
      ],
    });
    return !!(await this.#createAIEngine(service, aggregator));
  }

  async _mlDelete({ data: service }) {
    if (service !== IMAGE_TO_TEXT_TASK) {
      return null;
    }
    PdfJsTelemetry.report({
      type: "editing",
      data: {
        type: "stamp",
        action: "pdfjs.image.alt_text.model_deleted",
      },
    });
    if (Cu.isInAutomation) {
      return null;
    }
    try {
      // TODO: Temporary workaround to delete the model from the cache.
      //       See bug 1908941.
      await lazy.EngineProcess.destroyMLEngine();
      const cache = await lazy.IndexedDBCache.init();
      await cache.deleteModels({
        taskName: service,
      });
    } catch (e) {
      console.error("Failed to delete AI model", e);
    }

    return null;
  }

  async #createAIEngine(taskName, aggregator) {
    try {
      return await lazy.createEngine(
        { engineId: ML_ENGINE_ID, taskName },
        aggregator?.aggregateCallback.bind(aggregator) || null
      );
    } catch (e) {
      console.error("Failed to create AI engine", e);
      return null;
    }
  }

  _saveURL(aMsg) {
    const { blobUrl, originalUrl, filename } = aMsg.data;
    this.browser.ownerGlobal.saveURL(
      blobUrl /* aURL */,
      originalUrl /* aOriginalURL */,
      filename /* aFileName */,
      null /* aFilePickerTitleKey */,
      true /* aShouldBypassCache */,
      false /* aSkipPrompt */,
      null /* aReferrerInfo */,
      null /* aCookieJarSettings*/,
      null /* aSourceDocument */,
      lazy.PrivateBrowsingUtils.isBrowserPrivate(
        this.browser
      ) /* aIsContentWindowPrivate */,
      Services.scriptSecurityManager.getSystemPrincipal() /* aPrincipal */,
      () => {
        if (blobUrl.startsWith("blob:")) {
          URL.revokeObjectURL(blobUrl);
        }
        Services.obs.notifyObservers(null, "pdfjs:saveComplete");
      }
    );
  }

  _updateControlState(aMsg) {
    let data = aMsg.data;
    let browser = this.browser;
    let tabbrowser = browser.getTabBrowser();
    let tab = tabbrowser.getTabForBrowser(browser);
    tabbrowser.getFindBar(tab).then(fb => {
      if (!fb) {
        // The tab or window closed.
        return;
      }
      fb.updateControlState(data.result, data.findPrevious);

      if (
        data.result === Ci.nsITypeAheadFind.FIND_FOUND ||
        data.result === Ci.nsITypeAheadFind.FIND_WRAPPED ||
        (data.result === Ci.nsITypeAheadFind.FIND_PENDING &&
          !this._findFailedString)
      ) {
        this._findFailedString = null;
        lazy.SetClipboardSearchString(data.rawQuery);
      } else if (!this._findFailedString) {
        this._findFailedString = data.rawQuery;
        lazy.SetClipboardSearchString(data.rawQuery);
      }

      let searchLengthened;
      switch (data.result) {
        case Ci.nsITypeAheadFind.FIND_NOTFOUND:
          searchLengthened =
            data.rawQuery.length > this._lastNotFoundStringLength;
          this._lastNotFoundStringLength = data.rawQuery.length;

          if (searchLengthened && !data.entireWord) {
            playSound("not-found");
          }
          break;
        case Ci.nsITypeAheadFind.FIND_WRAPPED:
          playSound("wrapped");
          break;
        case Ci.nsITypeAheadFind.FIND_PENDING:
          break;
        default:
          this._lastNotFoundStringLength = 0;
      }

      const matchesCount = this._requestMatchesCount(data.matchesCount);
      fb.onMatchesCountResult(matchesCount);
    });
  }

  _updateMatchesCount(aMsg) {
    let data = aMsg.data;
    let browser = this.browser;
    let tabbrowser = browser.getTabBrowser();
    let tab = tabbrowser.getTabForBrowser(browser);
    tabbrowser.getFindBar(tab).then(fb => {
      if (!fb) {
        // The tab or window closed.
        return;
      }
      const matchesCount = this._requestMatchesCount(data);
      fb.onMatchesCountResult(matchesCount);
    });
  }

  _requestMatchesCount(data) {
    if (!data) {
      return { current: 0, total: 0 };
    }
    let result = {
      current: data.current,
      total: data.total,
      limit:
        typeof lazy.matchesCountLimit === "number" ? lazy.matchesCountLimit : 0,
    };
    if (result.total > result.limit) {
      result.total = -1;
    }
    return result;
  }

  handleEvent(aEvent) {
    const type = aEvent.type;
    // Handle the tab find initialized event specially:
    if (type == "TabFindInitialized") {
      let browser = aEvent.target.linkedBrowser;
      this._hookupEventListeners(browser);
      aEvent.target.removeEventListener(type, this);
      return;
    }

    if (type == "SwapDocShells") {
      this._removeEventListener();
      let newBrowser = aEvent.detail;
      newBrowser.addEventListener(
        "EndSwapDocShells",
        () => {
          this._hookupEventListeners(newBrowser);
        },
        { once: true }
      );
      return;
    }

    // Ignore events findbar events which arrive while the Pdfjs document is in
    // the BFCache.
    if (this.windowContext.isInBFCache) {
      return;
    }

    // To avoid forwarding the message as a CPOW, create a structured cloneable
    // version of the event for both performance, and ease of usage, reasons.
    let detail = null;
    if (type !== "findbarclose") {
      detail = {
        query: aEvent.detail.query,
        caseSensitive: aEvent.detail.caseSensitive,
        entireWord: aEvent.detail.entireWord,
        highlightAll: aEvent.detail.highlightAll,
        findPrevious: aEvent.detail.findPrevious,
        matchDiacritics: aEvent.detail.matchDiacritics,
      };
    }

    let browser = aEvent.currentTarget.browser;
    if (!this._boundToFindbar) {
      throw new Error(
        "FindEventManager was not bound for the current browser."
      );
    }
    browser.sendMessageToActor(
      "PDFJS:Child:handleEvent",
      { type, detail },
      "Pdfjs"
    );
    aEvent.preventDefault();
  }

  _addEventListener() {
    let browser = this.browser;
    if (this._boundToFindbar) {
      throw new Error(
        "FindEventManager was bound 2nd time without unbinding it first."
      );
    }

    this._hookupEventListeners(browser);
  }

  /**
   * Either hook up all the find event listeners if a findbar exists,
   * or listen for a find bar being created and hook up event listeners
   * when it does get created.
   */
  _hookupEventListeners(aBrowser) {
    let tabbrowser = aBrowser.getTabBrowser();
    let tab = tabbrowser.getTabForBrowser(aBrowser);
    let findbar = tabbrowser.getCachedFindBar(tab);
    if (findbar) {
      // And we need to start listening to find events.
      for (var i = 0; i < gFindTypes.length; i++) {
        var type = gFindTypes[i];
        findbar.addEventListener(type, this, true);
      }
      this._boundToFindbar = findbar;
    } else {
      tab.addEventListener("TabFindInitialized", this);
    }
    aBrowser.addEventListener("SwapDocShells", this);
    return !!findbar;
  }

  _removeEventListener() {
    let browser = this.browser;

    // make sure the listener has been removed.
    let findbar = this._boundToFindbar;
    if (findbar) {
      // No reason to listen to find events any longer.
      for (var i = 0; i < gFindTypes.length; i++) {
        var type = gFindTypes[i];
        findbar.removeEventListener(type, this, true);
      }
    } else if (browser) {
      // If we registered a `TabFindInitialized` listener which never fired,
      // make sure we remove it.
      let tabbrowser = browser.getTabBrowser();
      let tab = tabbrowser.getTabForBrowser(browser);
      tab?.removeEventListener("TabFindInitialized", this);
    }

    this._boundToFindbar = null;

    // Clean up any SwapDocShells event listeners.
    browser?.removeEventListener("SwapDocShells", this);
  }
}
