/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsManager: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/SettingsQueue.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function SettingsLock(aSettingsManager) {
  this._open = true;
  this._isBusy = false;
  this._requests = new Queue();
  this._settingsManager = aSettingsManager;
  this._transaction = null;
}

SettingsLock.prototype = {
  get closed() {
    return !this._open;
  },

  _wrap: function _wrap(obj) {
    return Cu.cloneInto(obj, this._settingsManager._window);
  },

  process: function process() {
    let lock = this;
    let store = lock._transaction.objectStore(SETTINGSSTORE_NAME);

    while (!lock._requests.isEmpty()) {
      let info = lock._requests.dequeue();
      if (DEBUG) debug("info: " + info.intent);
      let request = info.request;
      switch (info.intent) {
        case "clear":
          let clearReq = store.clear();
          clearReq.onsuccess = function() {
            this._open = true;
            Services.DOMRequest.fireSuccess(request, 0);
            this._open = false;
          }.bind(lock);
          clearReq.onerror = function() {
            Services.DOMRequest.fireError(request, 0)
          };
          break;
        case "set":
          let keys = Object.getOwnPropertyNames(info.settings);
          for (let i = 0; i < keys.length; i++) {
            let key = keys[i];
            let last = i === keys.length - 1;
            if (DEBUG) debug("key: " + key + ", val: " + JSON.stringify(info.settings[key]) + ", type: " + typeof(info.settings[key]));
            lock._isBusy = true;
            let checkKeyRequest = store.get(key);

            checkKeyRequest.onsuccess = function (event) {
              let defaultValue;
              let userValue = info.settings[key];
              if (event.target.result) {
                defaultValue = event.target.result.defaultValue;
              } else {
                defaultValue = null;
                if (DEBUG) debug("MOZSETTINGS-SET-WARNING: " + key + " is not in the database.\n");
              }

              let obj = {settingName: key, defaultValue: defaultValue, userValue: userValue};
              if (DEBUG) debug("store1: " + JSON.stringify(obj));
              let setReq = store.put(obj);

              setReq.onsuccess = function() {
                lock._isBusy = false;
                cpmm.sendAsyncMessage("Settings:Changed", { key: key, value: userValue });
                if (last && !request.error) {
                  lock._open = true;
                  Services.DOMRequest.fireSuccess(request, 0);
                  lock._open = false;
                  if (!lock._requests.isEmpty()) {
                    lock.process();
                  }
                }
              };

              setReq.onerror = function() {
                if (!request.error) {
                  Services.DOMRequest.fireError(request, setReq.error.name)
                }
              };
            };
            checkKeyRequest.onerror = function(event) {
              if (!request.error) {
                Services.DOMRequest.fireError(request, checkKeyRequest.error.name)
              }
            };
          }
          break;
        case "get":
          let getReq = (info.name === "*") ? store.mozGetAll()
                                           : store.mozGetAll(info.name);

          getReq.onsuccess = function(event) {
            if (DEBUG) debug("Request for '" + info.name + "' successful. " +
                             "Record count: " + event.target.result.length);

            if (event.target.result.length == 0) {
              if (DEBUG) debug("MOZSETTINGS-GET-WARNING: " + info.name + " is not in the database.\n");
            }

            let results = {};

            for (var i in event.target.result) {
              let result = event.target.result[i];
              var name = result.settingName;
              if (DEBUG) debug("VAL: " + result.userValue +", " + result.defaultValue + "\n");
              var value = result.userValue !== undefined ? result.userValue : result.defaultValue;
              results[name] = this._wrap(value);
            }

            this._open = true;
            Services.DOMRequest.fireSuccess(request, this._wrap(results));
            this._open = false;
          }.bind(lock);

          getReq.onerror = function() {
            Services.DOMRequest.fireError(request, 0)
          };
          break;
      }
    }
  },

  createTransactionAndProcess: function() {
    if (this._settingsManager._settingsDB._db) {
      var lock;
      while (lock = this._settingsManager._locks.dequeue()) {
        if (!lock._transaction) {
          let transactionType = this._settingsManager.hasWritePrivileges ? "readwrite" : "readonly";
          lock._transaction = lock._settingsManager._settingsDB._db.transaction(SETTINGSSTORE_NAME, transactionType);
        }
        if (!lock._isBusy) {
          lock.process();
        } else {
          this._settingsManager._locks.enqueue(lock);
        }
      }
      if (!this._requests.isEmpty() && !this._isBusy) {
        this.process();
      }
    }
  },

  get: function get(aName) {
    if (!this._open) {
      dump("Settings lock not open!\n");
      throw Components.results.NS_ERROR_ABORT;
    }

    if (this._settingsManager.hasReadPrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent:"get", name: aName });
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("get not allowed");
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }
  },

  _serializePreservingBinaries: function _serializePreservingBinaries(aObject) {
    // We need to serialize settings objects, otherwise they can change between
    // the set() call and the enqueued request being processed. We can't simply
    // parse(stringify(obj)) because that breaks things like Blobs, Files and
    // Dates, so we use stringify's replacer and parse's reviver parameters to
    // preserve binaries.
    let manager = this._settingsManager;
    let binaries = Object.create(null);
    let stringified = JSON.stringify(aObject, function(key, value) {
      value = manager._settingsDB.prepareValue(value);
      let kind = ObjectWrapper.getObjectKind(value);
      if (kind == "file" || kind == "blob" || kind == "date") {
        let uuid = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
                                                      .generateUUID().toString();
        binaries[uuid] = value;
        return uuid;
      }
      return value;
    });
    return JSON.parse(stringified, function(key, value) {
      if (value in binaries) {
        return binaries[value];
      }
      return value;
    });
  },

  set: function set(aSettings) {
    if (!this._open) {
      throw "Settings lock not open";
    }

    if (this._settingsManager.hasWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      if (DEBUG) debug("send: " + JSON.stringify(aSettings));
      let settings = this._serializePreservingBinaries(aSettings);
      this._requests.enqueue({request: req, intent: "set", settings: settings});
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("set not allowed");
      throw "No permission to call set";
    }
  },

  clear: function clear() {
    if (!this._open) {
      throw "Settings lock not open";
    }

    if (this._settingsManager.hasWritePrivileges) {
      let req = Services.DOMRequest.createRequest(this._settingsManager._window);
      this._requests.enqueue({ request: req, intent: "clear"});
      this.createTransactionAndProcess();
      return req;
    } else {
      if (DEBUG) debug("clear not allowed");
      throw "No permission to call clear";
    }
  },

  classID: Components.ID("{60c9357c-3ae0-4222-8f55-da01428470d5}"),
  contractID: "@mozilla.org/settingsLock;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
};

function SettingsManager() {
  this._locks = new Queue();
  this._settingsDB = new SettingsDB();
  this._settingsDB.init();
}

SettingsManager.prototype = {
  _callbacks: null,

  _wrap: function _wrap(obj) {
    return Cu.cloneInto(obj, this._window);
  },

  nextTick: function nextTick(aCallback, thisObj) {
    if (thisObj)
      aCallback = aCallback.bind(thisObj);

    Services.tm.currentThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  set onsettingchange(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onsettingchange", aHandler);
  },

  get onsettingchange() {
    return this.__DOM_IMPL__.getEventHandler("onsettingchange");
  },

  createLock: function() {
    if (DEBUG) debug("get lock!");
    var lock = new SettingsLock(this);
    this._locks.enqueue(lock);
    this._settingsDB.ensureDB(
      function() { lock.createTransactionAndProcess(); },
      function() { dump("Cannot open Settings DB. Trying to open an old version?\n"); }
    );
    this.nextTick(function() { this._open = false; }, lock);
    return lock;
  },

  receiveMessage: function(aMessage) {
    if (DEBUG) debug("Settings::receiveMessage: " + aMessage.name);
    let msg = aMessage.json;

    switch (aMessage.name) {
      case "Settings:Change:Return:OK":
        if (DEBUG) debug('data:' + msg.key + ':' + msg.value + '\n');

        let event = new this._window.MozSettingsEvent("settingchange", this._wrap({
          settingName: msg.key,
          settingValue: msg.value
        }));
        this.__DOM_IMPL__.dispatchEvent(event);

        if (this._callbacks && this._callbacks[msg.key]) {
          if (DEBUG) debug("observe callback called! " + msg.key + " " + this._callbacks[msg.key].length);
          this._callbacks[msg.key].forEach(function(cb) {
            cb(this._wrap({settingName: msg.key, settingValue: msg.value}));
          }.bind(this));
        } else {
          if (DEBUG) debug("no observers stored!");
        }
        break;
      default:
        if (DEBUG) debug("Wrong message: " + aMessage.name);
    }
  },

  addObserver: function addObserver(aName, aCallback) {
    if (DEBUG) debug("addObserver " + aName);
    if (!this._callbacks) {
      cpmm.sendAsyncMessage("Settings:RegisterForMessages");
      this._callbacks = {};
    }
    if (!this._callbacks[aName]) {
      this._callbacks[aName] = [aCallback];
    } else {
      this._callbacks[aName].push(aCallback);
    }
  },

  removeObserver: function removeObserver(aName, aCallback) {
    if (DEBUG) debug("deleteObserver " + aName);
    if (this._callbacks && this._callbacks[aName]) {
      let index = this._callbacks[aName].indexOf(aCallback)
      if (index != -1) {
        this._callbacks[aName].splice(index, 1)
      } else {
        if (DEBUG) debug("Callback not found for: " + aName);
      }
    } else {
      if (DEBUG) debug("No observers stored for " + aName);
    }
  },

  init: function(aWindow) {
    cpmm.addMessageListener("Settings:Change:Return:OK", this);
    this._window = aWindow;
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    this.innerWindowID = util.currentInnerWindowID;

    let readPerm = Services.perms.testExactPermissionFromPrincipal(aWindow.document.nodePrincipal, "settings-read");
    let writePerm = Services.perms.testExactPermissionFromPrincipal(aWindow.document.nodePrincipal, "settings-write");
    this.hasReadPrivileges = readPerm == Ci.nsIPermissionManager.ALLOW_ACTION;
    this.hasWritePrivileges = writePerm == Ci.nsIPermissionManager.ALLOW_ACTION;

    if (this.hasReadPrivileges) {
      cpmm.sendAsyncMessage("Settings:RegisterForMessages");
    }

    if (!this.hasReadPrivileges && !this.hasWritePrivileges) {
      dump("No settings permission for: " + aWindow.document.nodePrincipal.origin + "\n");
      Cu.reportError("No settings permission for: " + aWindow.document.nodePrincipal.origin);
    }
  },

  observe: function(aSubject, aTopic, aData) {
    if (DEBUG) debug("Topic: " + aTopic);
    if (aTopic == "inner-window-destroyed") {
      let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (wId == this.innerWindowID) {
        Services.obs.removeObserver(this, "inner-window-destroyed");
        cpmm.removeMessageListener("Settings:Change:Return:OK", this);
        this._requests = null;
        this._window = null;
        this._innerWindowID = null;
        this._settingsDB.close();
      }
    }
  },

  classID: Components.ID("{c40b1c70-00fb-11e2-a21f-0800200c9a66}"),
  contractID: "@mozilla.org/settingsManager;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsManager, SettingsLock])
