/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

/* static functions */
let DEBUG = 0;
let debug;
if (DEBUG)
  debug = function (s) { dump("-*- SettingsService: " + s + "\n"); }
else
  debug = function (s) {}

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/SettingsQueue.jsm");
Cu.import("resource://gre/modules/SettingsDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const nsIClassInfo            = Ci.nsIClassInfo;

const SETTINGSSERVICELOCK_CONTRACTID = "@mozilla.org/settingsServiceLock;1";
const SETTINGSSERVICELOCK_CID        = Components.ID("{d7a395a0-e292-11e1-834e-1761d57f5f99}");
const nsISettingsServiceLock         = Ci.nsISettingsServiceLock;

function SettingsServiceLock(aSettingsService, aTransactionCallback)
{
  if (DEBUG) debug("settingsServiceLock constr!");
  this._open = true;
  this._busy = false;
  this._requests = new Queue();
  this._settingsService = aSettingsService;
  this._transaction = null;
  this._transactionCallback = aTransactionCallback;
}

SettingsServiceLock.prototype = {

  callHandle: function callHandle(aCallback, aName, aValue) {
    try {
      aCallback ? aCallback.handle(aName, aValue) : null;
    } catch (e) {
      dump("settings 'handle' callback threw an exception, dropping: " + e + "\n");
    }
  },

  callAbort: function callAbort(aCallback, aMessage) {
    try {
      aCallback ? aCallback.handleAbort(aMessage) : null;
    } catch (e) {
      dump("settings 'abort' callback threw an exception, dropping: " + e + "\n");
    }
  },

  callError: function callError(aCallback, aMessage) {
    try {
      aCallback ? aCallback.handleError(aMessage) : null;
    } catch (e) {
      dump("settings 'error' callback threw an exception, dropping: " + e + "\n");
    }
  },

  callTransactionHandle: function callTransactionHandle() {
    try {
      this._transactionCallback ? this._transactionCallback.handle() : null;
    } catch (e) {
      dump("settings 'Transaction handle' callback threw an exception, dropping: " + e + "\n");
    }
  },

  process: function process() {
    debug("process!");
    let lock = this;
    lock._open = false;
    let store = lock._transaction.objectStore(SETTINGSSTORE_NAME);

    while (!lock._requests.isEmpty()) {
      if (lock._isBusy) {
        return;
      }
      let info = lock._requests.dequeue();
      if (DEBUG) debug("info:" + info.intent);
      let callback = info.callback;
      let name = info.name;
      switch (info.intent) {
        case "set":
          let value = info.value;
          let message = info.message;
          if(DEBUG && typeof(value) == 'object') {
            debug("object name:" + name + ", val: " + JSON.stringify(value));
          }
          lock._isBusy = true;
          let checkKeyRequest = store.get(name);

          checkKeyRequest.onsuccess = function (event) {
            let defaultValue;
            if (event.target.result) {
              defaultValue = event.target.result.defaultValue;
            } else {
              defaultValue = null;
              if (DEBUG) debug("MOZSETTINGS-SET-WARNING: " + name + " is not in the database.\n");
            }
            let setReq = store.put({ settingName: name, defaultValue: defaultValue, userValue: value });

            setReq.onsuccess = function() {
              lock._isBusy = false;
              lock._open = true;
              lock.callHandle(callback, name, value);
              Services.obs.notifyObservers(lock, "mozsettings-changed", JSON.stringify({
                key: name,
                value: value,
                message: message
              }));
              lock._open = false;
              lock.process();
            };

            setReq.onerror = function(event) {
              lock._isBusy = false;
              lock.callError(callback, event.target.errorMessage);
              lock.process();
            };
          }

          checkKeyRequest.onerror = function(event) {
            lock._isBusy = false;
            lock.callError(callback, event.target.errorMessage);
            lock.process();
          };
          break;
        case "get":
          let getReq = store.mozGetAll(name);
          getReq.onsuccess = function(event) {
            if (DEBUG) {
              debug("Request successful. Record count:" + event.target.result.length);
              debug("result: " + JSON.stringify(event.target.result));
            }
            this._open = true;
            if (callback) {
              if (event.target.result[0]) {
                if (event.target.result.length > 1) {
                  if (DEBUG) debug("Warning: overloaded setting:" + name);
                }
                let result = event.target.result[0];
                let value = result.userValue !== undefined
                            ? result.userValue
                            : result.defaultValue;
                lock.callHandle(callback, name, value);
              } else {
                lock.callHandle(callback, name, null);
              }
            } else {
              if (DEBUG) debug("no callback defined!");
            }
            this._open = false;
          }.bind(lock);
          getReq.onerror = function error(event) {
            lock.callError(callback, event.target.errorMessage);
          };
          break;
      }
    }
    lock._open = true;
  },

  createTransactionAndProcess: function() {
    if (this._settingsService._settingsDB._db) {
      let lock;
      while (lock = this._settingsService._locks.dequeue()) {
        if (!lock._transaction) {
          lock._transaction = lock._settingsService._settingsDB._db.transaction(SETTINGSSTORE_NAME, "readwrite");
          if (lock._transactionCallback) {
            lock._transaction.oncomplete = lock.callTransactionHandle.bind(lock);
            lock._transaction.onabort = function(event) {
              let message = '';
              if (event.target.error) {
                message = event.target.error.name + ': ' + event.target.error.message;
              }
              this.callAbort(lock._transactionCallback.handleAbort, message);
            };
          }
        }
        if (!lock._isBusy) {
          lock.process();
        } else {
          this._settingsService._locks.enqueue(lock);
          return;
        }
      }
      if (!this._requests.isEmpty() && !this._isBusy) {
        this.process();
      }
    }
  },

  get: function get(aName, aCallback) {
    if (DEBUG) debug("get: " + aName + ", " + aCallback);
    this._requests.enqueue({ callback: aCallback, intent:"get", name: aName });
    this.createTransactionAndProcess();
  },

  set: function set(aName, aValue, aCallback, aMessage) {
    debug("set: " + aName + ": " + JSON.stringify(aValue));
    if (aMessage === undefined)
      aMessage = null;
    this._requests.enqueue({ callback: aCallback,
                             intent: "set", 
                             name: aName, 
                             value: this._settingsService._settingsDB.prepareValue(aValue),
                             message: aMessage });
    this.createTransactionAndProcess();
  },

  classID : SETTINGSSERVICELOCK_CID,
  QueryInterface : XPCOMUtils.generateQI([nsISettingsServiceLock])
};

const SETTINGSSERVICE_CID        = Components.ID("{f656f0c0-f776-11e1-a21f-0800200c9a66}");

function SettingsService()
{
  debug("settingsService Constructor");
  this._locks = new Queue();
  this._settingsDB = new SettingsDB();
  this._settingsDB.init();
}

SettingsService.prototype = {

  nextTick: function nextTick(aCallback, thisObj) {
    if (thisObj)
      aCallback = aCallback.bind(thisObj);

    Services.tm.currentThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  createLock: function createLock(aCallback) {
    var lock = new SettingsServiceLock(this, aCallback);
    this._locks.enqueue(lock);
    this._settingsDB.ensureDB(
      function() { lock.createTransactionAndProcess(); },
      function() { dump("SettingsService failed to open DB!\n"); }
    );
    this.nextTick(function() { this._open = false; }, lock);
    return lock;
  },

  classID : SETTINGSSERVICE_CID,
  QueryInterface : XPCOMUtils.generateQI([Ci.nsISettingsService])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SettingsService, SettingsServiceLock])
