/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabStateFlusher"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Promise.jsm", this);

/**
 * A module that enables async flushes. Updates from frame scripts are
 * throttled to be sent only once per second. If an action wants a tab's latest
 * state without waiting for a second then it can request an async flush and
 * wait until the frame scripts reported back. At this point the parent has the
 * latest data and the action can continue.
 */
this.TabStateFlusher = Object.freeze({
  /**
   * Requests an async flush for the given browser. Returns a promise that will
   * resolve when we heard back from the content process and the parent has
   * all the latest data.
   */
  flush(browser) {
    return TabStateFlusherInternal.flush(browser);
  },

  /**
   * Resolves the flush request with the given flush ID.
   */
  resolve(browser, flushID) {
    TabStateFlusherInternal.resolve(browser, flushID);
  },

  /**
   * Resolves all active flush requests for a given browser. This should be
   * used when the content process crashed or the final update message was
   * seen. In those cases we can't guarantee to ever hear back from the frame
   * script so we just resolve all requests instead of discarding them.
   */
  resolveAll(browser) {
    TabStateFlusherInternal.resolveAll(browser);
  }
});

let TabStateFlusherInternal = {
  // Stores the last request ID.
  _lastRequestID: 0,

  // A map storing all active requests per browser.
  _requests: new WeakMap(),

  /**
   * Requests an async flush for the given browser. Returns a promise that will
   * resolve when we heard back from the content process and the parent has
   * all the latest data.
   */
  flush(browser) {
    let id = ++this._lastRequestID;
    let mm = browser.messageManager;
    mm.sendAsyncMessage("SessionStore:flush", {id});

    // Retrieve active requests for given browser.
    let permanentKey = browser.permanentKey;
    let perBrowserRequests = this._requests.get(permanentKey) || new Map();

    return new Promise(resolve => {
      // Store resolve() so that we can resolve the promise later.
      perBrowserRequests.set(id, resolve);

      // Update the flush requests stored per browser.
      this._requests.set(permanentKey, perBrowserRequests);
    });
  },

  /**
   * Resolves the flush request with the given flush ID.
   */
  resolve(browser, flushID) {
    // Nothing to do if there are no pending flushes for the given browser.
    if (!this._requests.has(browser.permanentKey)) {
      return;
    }

    // Retrieve active requests for given browser.
    let perBrowserRequests = this._requests.get(browser.permanentKey);
    if (!perBrowserRequests.has(flushID)) {
      return;
    }

    // Resolve the request with the given id.
    let resolve = perBrowserRequests.get(flushID);
    perBrowserRequests.delete(flushID);
    resolve();
  },

  /**
   * Resolves all active flush requests for a given browser. This should be
   * used when the content process crashed or the final update message was
   * seen. In those cases we can't guarantee to ever hear back from the frame
   * script so we just resolve all requests instead of discarding them.
   */
  resolveAll(browser) {
    // Nothing to do if there are no pending flushes for the given browser.
    if (!this._requests.has(browser.permanentKey)) {
      return;
    }

    // Retrieve active requests for given browser.
    let perBrowserRequests = this._requests.get(browser.permanentKey);

    // Resolve all requests.
    for (let resolve of perBrowserRequests.values()) {
      resolve();
    }

    // Clear active requests.
    perBrowserRequests.clear();
  }
};
