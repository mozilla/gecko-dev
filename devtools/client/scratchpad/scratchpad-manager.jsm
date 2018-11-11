/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ScratchpadManager"];

const SCRATCHPAD_WINDOW_URL = "chrome://devtools/content/scratchpad/index.xul";
const SCRATCHPAD_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const Telemetry = require("devtools/client/shared/telemetry");

const TELEMETRY_SCRATCHPAD_WIN_OPEN_COUNT = "DEVTOOLS_SCRATCHPAD_WINDOW_OPENED_COUNT";

/**
 * The ScratchpadManager object opens new Scratchpad windows and manages the state
 * of open scratchpads for session restore. There's only one ScratchpadManager in
 * the life of the browser.
 */
this.ScratchpadManager = {

  _nextUid: 1,
  _scratchpads: [],

  _telemetry: new Telemetry(),

  /**
   * Get the saved states of open scratchpad windows. Called by
   * session restore.
   *
   * @return array
   *         The array of scratchpad states.
   */
  getSessionState: function SPM_getSessionState() {
    return this._scratchpads;
  },

  /**
   * Restore scratchpad windows from the scratchpad session store file.
   * Called by session restore.
   *
   * @param function aSession
   *        The session object with scratchpad states.
   *
   * @return array
   *         The restored scratchpad windows.
   */
  restoreSession: function SPM_restoreSession(aSession) {
    if (!Array.isArray(aSession)) {
      return [];
    }

    const wins = [];
    aSession.forEach(function(state) {
      const win = this.openScratchpad(state);
      wins.push(win);
    }, this);

    return wins;
  },

  /**
   * Iterate through open scratchpad windows and save their states.
   */
  saveOpenWindows: function SPM_saveOpenWindows() {
    this._scratchpads = [];

    function clone(src) {
      const dest = {};

      for (const key in src) {
        if (src.hasOwnProperty(key)) {
          dest[key] = src[key];
        }
      }

      return dest;
    }

    // We need to clone objects we get from Scratchpad instances
    // because such (cross-window) objects have a property 'parent'
    // that holds on to a ChromeWindow instance. This means that
    // such objects are not primitive-values-only anymore so they
    // can leak.

    for (const win of Services.wm.getEnumerator("devtools:scratchpad")) {
      if (!win.closed && win.Scratchpad.initialized) {
        this._scratchpads.push(clone(win.Scratchpad.getState()));
      }
    }
  },

  /**
   * Open a new scratchpad window with an optional initial state.
   *
   * @param object aState
   *        Optional. The initial state of the scratchpad, an object
   *        with properties filename, text, and executionContext.
   *
   * @return nsIDomWindow
   *         The opened scratchpad window.
   */
  openScratchpad: function SPM_openScratchpad(aState) {
    const params = Cc["@mozilla.org/embedcomp/dialogparam;1"]
                 .createInstance(Ci.nsIDialogParamBlock);

    params.SetNumberStrings(2);
    params.SetString(0, this.createUid());

    if (aState) {
      if (typeof aState != "object") {
        return;
      }

      params.SetString(1, JSON.stringify(aState));
    }

    const win = Services.ww.openWindow(null, SCRATCHPAD_WINDOW_URL, "_blank",
                                     SCRATCHPAD_WINDOW_FEATURES, params);

    this._telemetry.getHistogramById(TELEMETRY_SCRATCHPAD_WIN_OPEN_COUNT).add(true);

    // Only add the shutdown observer if we've opened a scratchpad window.
    ShutdownObserver.init();

    return win;
  },

  /**
   * Create a unique ID for a new Scratchpad.
   */
  createUid: function SPM_createUid() {
    return JSON.stringify(this._nextUid++);
  },
};

/**
 * The ShutdownObserver listens for app shutdown and saves the current state
 * of the scratchpads for session restore.
 */
var ShutdownObserver = {
  _initialized: false,
  init() {
    if (this._initialized) {
      return;
    }

    Services.obs.addObserver(this, "quit-application-granted");

    this._initialized = true;
  },

  observe(message, topic) {
    if (topic == "quit-application-granted") {
      ScratchpadManager.saveOpenWindows();
      this.uninit();
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "quit-application-granted");
  },
};
