/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XXX TODO
 * unregister as much as possible when removing a customization.
 */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["UserCustomization"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "console",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");
/**
  * Customization scripts and CSS stylesheets can be specified in an
  * application manifest with the following syntax:
  * "customization": [
  *  {
  *    "filter": "http://youtube.com",
  *    "css": ["file1.css", "file2.css"],
  *    "scripts": ["script1.js", "script2.js"]
  *  }
  * ]
  */

function debug(aStr) {
  dump("-*-*- UserCustomization (" +
       (UserCustomization._inParent ? "parent" : "child") +
       "): " + aStr + "\n");
}

function log(aStr) {
  console.logStringMessage(aStr);
}

this.UserCustomization = {
  _items: [],

  _addItem: function(aItem) {
    debug("Registering item: " + uneval(aItem));
    this._items.push(aItem);
    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomization:Add", [aItem]);
    }
  },

  _removeItem: function(aHash) {
    debug("Unregistering item: " + aHash);
    let index = -1;
    this._items.forEach((script, pos) => {
      if (script.hash == aHash ) {
        index = pos;
      }
    });

    if (index != -1) {
      this._items.splice(index, 1);
    }

    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomization:Remove", aHash);
    }
  },

  register: function(aManifest, aApp) {
    let enabled = false;
    try {
      enabled = Services.prefs.getBoolPref("dom.apps.customization.enabled");
    } catch(e) {}
    if (!enabled) {
      return;
    }

    debug("Starting customization registration for " + aApp.origin);
    let customization = aManifest.customization;
    if (customization === undefined || !Array.isArray(customization)) {
      return;
    }

    let origin = Services.io.newURI(aApp.origin, null, null);

    customization.forEach((item) => {
      // The filter property is mandatory.
      // XXX do a format check? should this be regexp?
      if (!item.filter || (typeof item.filter !== "string")) {
        log("Mandatory filter property not found in this customization item: " +
            uneval(item) + " in " + aApp.manifestURL);
        return;
      }

      // Create a new object with resolved urls and a hash that we reuse to
      // remove items.
      let custom = {
        filter: item.filter,
        status: aApp.appStatus,
        css: [],
        scripts: []
      };
      custom.hash = AppsUtils.computeObjectHash(item);

      if (item.css && Array.isArray(item.css)) {
        item.css.forEach((css) => {
          custom.css.push(origin.resolve(css));
        });
      }

      if (item.scripts && Array.isArray(item.scripts)) {
        item.scripts.forEach((script) => {
          custom.scripts.push(origin.resolve(script));
        });
      }

      this._addItem(custom);
    });
  },

  unregister: function(aManifest, aApp) {
    debug("Starting customization unregistration for " + aApp.origin);
    let customization = aManifest.customization;
    if (customization === undefined || !Array.isArray(customization)) {
      return;
    }

    let origin = Services.io.newURI(aApp.origin, null, null);

    customization.forEach((item) => {
      this._removeItem(AppsUtils.computeObjectHash(item));
    });
  },

  _injectItem: function(aWindow, aItem) {
    debug("Injecting item " + uneval(aItem) + " in " + aWindow.location.href);
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);

    // Load the stylesheets only in this window.
    aItem.css.forEach((aCss) => {
      utils.loadSheet(Services.io.newURI(aCss, null, null),
                      Ci.nsIDOMWindowUtils.AUTHOR_SHEET);
    });

    let sandbox = Cu.Sandbox(aWindow,
                             { wantComponents: false,
                               wantXrays: true,
                               sandboxPrototype: aWindow });

    // Load the scripts using a sandbox.
    aItem.scripts.forEach((aScript) => {
      debug("Sandboxing " + aScript);
      try {
        Services.scriptloader.loadSubScript(aScript, sandbox, "UTF-8");
      } catch(e) {
        log("Error sandboxing " + aScript + " : " + e);
      }
    });

    // Makes sure we get rid of the sandbox.
    aWindow.addEventListener("unload", () => {
      Cu.nukeSandbox(sandbox);
      sandbox = null;
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "content-document-global-created") {
      let window = aSubject.QueryInterface(Ci.nsIDOMWindow);
      let href = window.location.href;
      if (!href || href == "about:blank") {
        return;
      }

      let principal = window.document.nodePrincipal;
      debug("document created: " + href);
      debug("principal status: " + principal.appStatus);

      this._items.forEach((aItem) => {
        // We only allow customizations to apply to apps with an equal or lower
        // privilege level.
        if (principal.appStatus > aItem.status) {
          return;
        }

        if (href.startsWith(aItem.filter)) {
          this._injectItem(window, aItem);
        }
      });
    }
  },

  init: function() {
    debug("init");
    this._inParent = Cc["@mozilla.org/xre/runtime;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

    Services.obs.addObserver(this, "content-document-global-created",
                             /* ownsWeak */ false);

    if (this._inParent) {
      ppmm.addMessageListener("UserCustomization:List", this);
    } else {
      cpmm.addMessageListener("UserCustomization:Add", this);
      cpmm.addMessageListener("UserCustomization:Remove", this);
      cpmm.sendAsyncMessage("UserCustomization:List", {});
    }
  },

  receiveMessage: function(aMessage) {
    let name = aMessage.name;
    let data = aMessage.data;

    switch(name) {
      case "UserCustomization:List":
        aMessage.target.sendAsyncMessage("UserCustomization:Add", this._items);
        break;
      case "UserCustomization:Add":
        data.forEach(this._addItem, this);
        break;
      case "UserCustomization:Remove":
        this._removeItem(data);
        break;
    }
  }
}

UserCustomization.init();
