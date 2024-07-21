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

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  createEngine: "chrome://global/content/ml/EngineProcess.sys.mjs",
  MultiProgressAggregator: "chrome://global/content/ml/Utils.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PdfJsTelemetry: "resource://pdf.js/PdfJsTelemetry.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SetClipboardSearchString: "resource://gre/modules/Finder.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "PDF_JS",
  });
});

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
  #mutablePreferences = new Set(["enableAltText"]);

  constructor() {
    super();
    this._boundToFindbar = null;
    this._findFailedString = null;
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
    }
    return undefined;
  }

  /*
   * Internal
   */

  get browser() {
    return this.browsingContext.top.embedderElement;
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

  _reportTelemetry(aMsg) {
    lazy.PdfJsTelemetry.report(aMsg.data);
  }

  _initProgressBar(progressData) {
    lazy.console.debug("progess_from_pdfjs", progressData);
  }

  async _mlGuess({ data: { service, request } }) {
    if (!lazy.createEngine) {
      return null;
    }
    if (service !== "image-to-text") {
      throw new Error("Invalid service");
    }

    let aggregator = new lazy.MultiProgressAggregator({
      progressCallback: this._initProgressBar,
    });

    const callback = aggregator.aggregateCallback.bind(aggregator);

    // We are using the internal task name prefixed with moz-
    const engine = await lazy.createEngine(
      {
        taskName: "moz-image-to-text",
      },
      callback
    );
    return engine.run(request);
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
