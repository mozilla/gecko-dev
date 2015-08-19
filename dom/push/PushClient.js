/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
let gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled)
    dump("-*- PushClient.js: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

gDebuggingEnabled = Services.prefs.getBoolPref("dom.push.debug");

const kMessages = [
  "PushService:Register:OK",
  "PushService:Register:KO",
  "PushService:Registration:OK",
  "PushService:Registration:KO",
  "PushService:Unregister:OK",
  "PushService:Unregister:KO",
];

this.PushClient = function PushClient() {
  debug("PushClient created!");
  this._cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                 .getService(Ci.nsISyncMessageSender);
  this._requests = {};
  this.addListeners();
};

function dispatchRegistration(request, registration) {
  if (!registration) {
    request.onPushEndpoint(Cr.NS_OK, "", 0, null);
    return;
  }
  if (!registration.publicKey) {
    request.onPushEndpoint(Cr.NS_OK, registration.pushEndpoint, 0, null);
    return;
  }
  let keyBytes = new Uint8Array(registration.publicKey);
  request.onPushEndpoint(Cr.NS_OK, registration.pushEndpoint, keyBytes.length, keyBytes);
}

PushClient.prototype = {
  classID: Components.ID("{16042199-bec0-484a-9640-25ecc0c0a149}"),

  contractID: "@mozilla.org/push/PushClient;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference,
                                         Ci.nsIPushClient,
                                         Ci.nsIMessageListener,]),


  _getRandomId: function() {
    return Cc["@mozilla.org/uuid-generator;1"]
             .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  addRequest: function(data) {
    let id = this._getRandomId();
    this._requests[id] = data;
    return id;
  },

  takeRequest: function(requestId) {
    let d = this._requests[requestId];
    delete this._requests[requestId];
    return d;
  },

  addListeners: function() {
    for (let message of kMessages) {
      this._cpmm.addWeakMessageListener(message, this);
    }
  },

  subscribe: function(scope, principal, callback) {
    debug("subscribe() " + scope);
    let requestId = this.addRequest(callback);
    this._cpmm.sendAsyncMessage("Push:Register", {
                                scope: scope,
                                requestID: requestId,
                              }, null, principal);
  },

  unsubscribe: function(scope, principal, callback) {
    debug("unsubscribe() " + scope);
    let requestId = this.addRequest(callback);
    this._cpmm.sendAsyncMessage("Push:Unregister", {
                                scope: scope,
                                requestID: requestId,
                              }, null, principal);
  },

  getSubscription: function(scope, principal, callback) {
    debug("getSubscription()" + scope);
    let requestId = this.addRequest(callback);
    debug("Going to send  " + scope + " " + principal + " " + requestId);
    this._cpmm.sendAsyncMessage("Push:Registration", {
                                scope: scope,
                                requestID: requestId,
                              }, null, principal);
  },

  receiveMessage: function(aMessage) {

    let json = aMessage.data;
    let request = this.takeRequest(json.requestID);

    if (!request) {
      return;
    }

    debug("receiveMessage(): " + JSON.stringify(aMessage))
    switch (aMessage.name) {
      case "PushService:Register:OK":
      {
        dispatchRegistration(request, json);
        break;
      }
      case "PushService:Register:KO":
        request.onPushEndpoint(Cr.NS_ERROR_FAILURE, "", 0, null);
        break;
      case "PushService:Registration:OK":
      {
        dispatchRegistration(request, json.registration);
        break;
      }
      case "PushService:Registration:KO":
        request.onPushEndpoint(Cr.NS_ERROR_FAILURE, "", 0, null);
        break;
      case "PushService:Unregister:OK":
        if (typeof json.result !== "boolean") {
          debug("Expected boolean result from PushService!");
          request.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
          return;
        }

        request.onUnsubscribe(Cr.NS_OK, json.result);
        break;
      case "PushService:Unregister:KO":
        request.onUnsubscribe(Cr.NS_ERROR_FAILURE, false);
        break;
      default:
        debug("NOT IMPLEMENTED! receiveMessage for " + aMessage.name);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PushClient]);
