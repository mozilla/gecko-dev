/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module contains `HiddenFrame`, a class which creates a windowless browser,
 * and `HiddenBrowserManager` which is a singleton that can be used to manage
 * creating and using multiple hidden frames.
 */

const XUL_PAGE = Services.io.newURI("chrome://global/content/win.xhtml");

const gAllHiddenFrames = new Set();

// The screen sizes to use for the background browser created by
// `HiddenBrowserManager`.
const BACKGROUND_WIDTH = 1024;
const BACKGROUND_HEIGHT = 768;

let cleanupRegistered = false;
function ensureCleanupRegistered() {
  if (!cleanupRegistered) {
    cleanupRegistered = true;
    Services.obs.addObserver(function () {
      for (let hiddenFrame of gAllHiddenFrames) {
        hiddenFrame.destroy();
      }
    }, "xpcom-shutdown");
  }
}

/**
 * A hidden frame class. It takes care of creating a windowless browser and
 * passing the window containing a blank XUL <window> back.
 */
export class HiddenFrame {
  #frame = null;
  #browser = null;
  #listener = null;
  #webProgress = null;
  #deferred = null;

  /**
   * Gets the |contentWindow| of the hidden frame. Creates the frame if needed.
   *
   * @returns {Promise} Returns a promise which is resolved when the hidden frame has finished
   *          loading.
   */
  get() {
    if (!this.#deferred) {
      this.#deferred = Promise.withResolvers();
      this.#create();
    }

    return this.#deferred.promise;
  }

  /**
   * Fetch a sync ref to the window inside the frame (needed for the add-on SDK).
   *
   * @returns {DOMWindow}
   */
  getWindow() {
    this.get();
    return this.#browser.document.ownerGlobal;
  }

  /**
   * Destroys the browser, freeing resources.
   */
  destroy() {
    if (this.#browser) {
      if (this.#listener) {
        this.#webProgress.removeProgressListener(this.#listener);
        this.#listener = null;
        this.#webProgress = null;
      }
      this.#frame = null;
      this.#deferred = null;

      gAllHiddenFrames.delete(this);
      this.#browser.close();
      this.#browser = null;
    }
  }

  #create() {
    ensureCleanupRegistered();
    let chromeFlags = Ci.nsIWebBrowserChrome.CHROME_REMOTE_WINDOW;
    if (Services.appinfo.fissionAutostart) {
      chromeFlags |= Ci.nsIWebBrowserChrome.CHROME_FISSION_WINDOW;
    }
    this.#browser = Services.appShell.createWindowlessBrowser(
      true,
      chromeFlags
    );
    this.#browser.QueryInterface(Ci.nsIInterfaceRequestor);
    gAllHiddenFrames.add(this);
    this.#webProgress = this.#browser.getInterface(Ci.nsIWebProgress);
    this.#listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsIWebProgressListener2",
        "nsISupportsWeakReference",
      ]),
    };
    this.#listener.onStateChange = (wbp, request, stateFlags) => {
      if (!request) {
        return;
      }
      if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this.#webProgress.removeProgressListener(this.#listener);
        this.#listener = null;
        this.#webProgress = null;
        // Get the window reference via the document.
        this.#frame = this.#browser.document.ownerGlobal;
        this.#deferred.resolve(this.#frame);
      }
    };
    this.#webProgress.addProgressListener(
      this.#listener,
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
    let docShell = this.#browser.docShell;
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    docShell.createAboutBlankDocumentViewer(systemPrincipal, systemPrincipal);
    let browsingContext = this.#browser.browsingContext;
    browsingContext.useGlobalHistory = false;
    let loadURIOptions = {
      triggeringPrincipal: systemPrincipal,
    };
    this.#browser.loadURI(XUL_PAGE, loadURIOptions);
  }
}

/**
 * A manager for hidden browsers. Responsible for creating and destroying a
 * hidden frame to hold them.
 */
export const HiddenBrowserManager = new (class HiddenBrowserManager {
  /**
   * The hidden frame if one has been created.
   *
   * @type {HiddenFrame | null}
   */
  #frame = null;
  /**
   * The number of hidden browser elements currently in use.
   *
   * @type {number}
   */
  #browsers = 0;

  /**
   * Creates and returns a new hidden browser.
   *
   * @returns {Browser}
   */
  async #acquireBrowser() {
    this.#browsers++;
    if (!this.#frame) {
      this.#frame = new HiddenFrame();
    }

    let frame = await this.#frame.get();
    let doc = frame.document;
    let browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute(
      "style",
      `
        width: ${BACKGROUND_WIDTH}px;
        min-width: ${BACKGROUND_WIDTH}px;
        height: ${BACKGROUND_HEIGHT}px;
        min-height: ${BACKGROUND_HEIGHT}px;
      `
    );
    browser.setAttribute("maychangeremoteness", "true");
    doc.documentElement.appendChild(browser);

    return browser;
  }

  /**
   * Releases the given hidden browser.
   *
   * @param {Browser} browser
   *   The hidden browser element.
   */
  #releaseBrowser(browser) {
    browser.remove();

    this.#browsers--;
    if (this.#browsers == 0) {
      this.#frame.destroy();
      this.#frame = null;
    }
  }

  /**
   * Calls a callback function with a new hidden browser.
   * This function will return whatever the callback function returns.
   *
   * @param {Callback} callback
   *   The callback function will be called with the browser element and may
   *   be asynchronous.
   * @returns {T}
   */
  async withHiddenBrowser(callback) {
    let browser = await this.#acquireBrowser();
    try {
      return await callback(browser);
    } finally {
      this.#releaseBrowser(browser);
    }
  }
})();
