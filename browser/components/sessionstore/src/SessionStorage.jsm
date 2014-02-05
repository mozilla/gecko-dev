/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionStorage"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");

// Returns the principal for a given |frame| contained in a given |docShell|.
function getPrincipalForFrame(docShell, frame) {
  let ssm = Services.scriptSecurityManager;
  let uri = frame.document.documentURIObject;
  return ssm.getDocShellCodebasePrincipal(uri, docShell);
}

this.SessionStorage = Object.freeze({
  /**
   * Updates all sessionStorage "super cookies"
   * @param docShell
   *        That tab's docshell (containing the sessionStorage)
   * @param frameTree
   *        The docShell's FrameTree instance.
   * @return Returns a nested object that will have hosts as keys and per-host
   *         session storage data as values. For example:
   *         {"example.com": {"key": "value", "my_number": 123}}
   */
  collect: function (docShell, frameTree) {
    return SessionStorageInternal.collect(docShell, frameTree);
  },

  /**
   * Restores all sessionStorage "super cookies".
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        A nested object with storage data to be restored that has hosts as
   *        keys and per-host session storage data as values. For example:
   *        {"example.com": {"key": "value", "my_number": 123}}
   */
  restore: function (aDocShell, aStorageData) {
    SessionStorageInternal.restore(aDocShell, aStorageData);
  }
});

let SessionStorageInternal = {
  /**
   * Reads all session storage data from the given docShell.
   * @param docShell
   *        A tab's docshell (containing the sessionStorage)
   * @param frameTree
   *        The docShell's FrameTree instance.
   * @return Returns a nested object that will have hosts as keys and per-host
   *         session storage data as values. For example:
   *         {"example.com": {"key": "value", "my_number": 123}}
   */
  collect: function (docShell, frameTree) {
    let data = {};
    let visitedOrigins = new Set();

    frameTree.forEach(frame => {
      let principal = getPrincipalForFrame(docShell, frame);
      if (!principal) {
        return;
      }

      // Get the root domain of the current history entry
      // and use that as a key for the per-host storage data.
      let origin = principal.jarPrefix + principal.origin;
      if (visitedOrigins.has(origin)) {
        // Don't read a host twice.
        return;
      }

      // Mark the current origin as visited.
      visitedOrigins.add(origin);

      let originData = this._readEntry(principal, docShell);
      if (Object.keys(originData).length) {
        data[origin] = originData;
      }
    });

    return Object.keys(data).length ? data : null;
  },

  /**
   * Writes session storage data to the given tab.
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        A nested object with storage data to be restored that has hosts as
   *        keys and per-host session storage data as values. For example:
   *        {"example.com": {"key": "value", "my_number": 123}}
   */
  restore: function (aDocShell, aStorageData) {
    for (let host of Object.keys(aStorageData)) {
      let data = aStorageData[host];
      let uri = Services.io.newURI(host, null, null);
      let principal = Services.scriptSecurityManager.getDocShellCodebasePrincipal(uri, aDocShell);
      let storageManager = aDocShell.QueryInterface(Ci.nsIDOMStorageManager);

      // There is no need to pass documentURI, it's only used to fill documentURI property of
      // domstorage event, which in this case has no consumer. Prevention of events in case
      // of missing documentURI will be solved in a followup bug to bug 600307.
      let storage = storageManager.createStorage(principal, "", aDocShell.usePrivateBrowsing);

      for (let key of Object.keys(data)) {
        try {
          storage.setItem(key, data[key]);
        } catch (e) {
          // throws e.g. for URIs that can't have sessionStorage
          console.error(e);
        }
      }
    }
  },

  /**
   * Reads an entry in the session storage data contained in a tab's history.
   * @param aURI
   *        That history entry uri
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   */
  _readEntry: function (aPrincipal, aDocShell) {
    let hostData = {};
    let storage;

    try {
      let storageManager = aDocShell.QueryInterface(Ci.nsIDOMStorageManager);
      storage = storageManager.getStorage(aPrincipal);
    } catch (e) {
      // sessionStorage might throw if it's turned off, see bug 458954
    }

    if (storage && storage.length) {
       for (let i = 0; i < storage.length; i++) {
        try {
          let key = storage.key(i);
          hostData[key] = storage.getItem(key);
        } catch (e) {
          // This currently throws for secured items (cf. bug 442048).
        }
      }
    }

    return hostData;
  }
};
