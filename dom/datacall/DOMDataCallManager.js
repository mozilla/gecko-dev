/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/DOMRequestHelper.jsm");

const DATACALLMANAGER_CONTRACTID = "@mozilla.org/datacallmanager;1;"
const DATACALLMANAGER_CID        = Components.ID("{73e0d4e0-bd62-489c-b67a-0140f95a1b24}");
const DATACALL_CONTRACTID        = "@mozilla.org/datacall;1";
const DATACALL_CID               = Components.ID("{b5ff4d17-1fa0-44a0-bc72-0047b5bb13c6}");

const TOPIC_PREF_CHANGED         = "nsPref:changed";
const PREF_RIL_DEBUG_ENABLED     = "ril.debugging.enabled";

const DATACALLMANAGER_IPC_MSG_ENTRIES = [
  "DataCall:RequestDataCall:Resolved",
  "DataCall:RequestDataCall:Rejected",
  "DataCall:GetDataCallState:Resolved",
  "DataCall:GetDataCallState:Rejected"
];

const DATACALL_IPC_MSG_ENTRIES = [
  "DataCall:AddHostRoute:Resolved",
  "DataCall:AddHostRoute:Rejected",
  "DataCall:ReleaseDataCall:Resolved",
  "DataCall:ReleaseDataCall:Rejected",
  "DataCall:RemoveHostRoute:Resolved",
  "DataCall:RemoveHostRoute:Rejected",
  "DataCall:OnStateChanged"
];

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

/* global RIL */
XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

XPCOMUtils.defineLazyGetter(this, "gDataCallHelper", function() {
  return {
    // Should match with enum DataCallState in DataCallManager webidl.
    convertToDataCallState: function(aState, aReason) {
      switch (aState) {
        case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTING:
          return "connecting";
        case Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED:
          return "connected";
        case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTING:
          return "disconnecting";
        case Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED:
          // If state is DISCONNECTED and reason is other than REASON_NONE,
          // implies the mobile network is no longer available. We'll set state
          // as 'unavailable' and block all subsequent requests to the DataCall.
          if (aReason !== undefined &&
              aReason !== Ci.nsINetworkInterface.REASON_NONE) {
            return "unavailable";
          }
          return "disconnected";
        default:
          return "unknown";
      }
    },

    // Should match with enum DataCallType in DataCallManager webidl.
    convertToDataCallType: function(aNetworkType) {
      switch (aNetworkType) {
        case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS:
          return "mms";
        case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL:
          return "supl";
        case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS:
          return "ims";
        case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN:
          return "dun";
        case Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_FOTA:
          return "fota";
        default:
          return "unknown";
      }
    },

    convertToNetworkType: function(aType) {
      switch (aType) {
        case "mms":
          return Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS;
        case "supl":
          return Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL;
        case "ims":
          return Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS;
        case "dun":
          return Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN;
        case "fota":
          return Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_FOTA;
        default:
          return Ci.nsINetworkInterface.NETWORK_TYPE_UNKNOWN;
      }
    },
  };
});

// set to true in ril_consts.js to see debug messages
let DEBUG = RIL.DEBUG_RIL;

function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref;
  try {
    debugPref = Services.prefs.getBoolPref(PREF_RIL_DEBUG_ENABLED);
  } catch (e) {
    debugPref = false;
  }
  DEBUG = debugPref || RIL.DEBUG_RIL;
}
updateDebugFlag();


function DOMDataCallManager() {
  Services.prefs.addObserver(PREF_RIL_DEBUG_ENABLED, this, false);
}
DOMDataCallManager.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  _window: null,

  _innerWindowId: null,

  classDescription: "DOMDataCallManager",
  classID: DATACALLMANAGER_CID,
  contractID: DATACALLMANAGER_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  debug: function(aMsg) {
    dump("-*- DOMDataCallManager: " + aMsg + "\n");
  },

  /*
   * nsIDOMGlobalPropertyInitializer implementation.
   */
  init: function(aWindow) {
    this._window = aWindow;

    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(
      Ci.nsIDOMWindowUtils);
    this._innerWindowId = utils.currentInnerWindowID;

    this.initDOMRequestHelper(aWindow, DATACALLMANAGER_IPC_MSG_ENTRIES);
  },

  /*
   * Called from DOMRequestIpcHelper.
   */
  uninit: function() {
    // All requests that are still pending need to be invalidated
    // because the context is no longer valid.
    this.forEachPromiseResolver(aKey => {
      this.takePromiseResolver(aKey).reject("DataCallManager got destroyed.");
    });
  },

  /*
   * Message manager handler
   */
  receiveMessage: function(aMessage) {
    if (DEBUG) {
      this.debug("Received message '" + aMessage.name + "': " +
                 JSON.stringify(aMessage.json));
    }

    let data = aMessage.data;
    let resolver = this.takePromiseResolver(data.resolverId);
    if (!resolver) {
      return;
    }

    switch (aMessage.name) {
      case "DataCall:RequestDataCall:Resolved": {
        let dataCall = data.result;
        let domDataCall = new DOMDataCall(this._window, this._innerWindowId,
                                          dataCall.serviceId,
                                          gDataCallHelper.convertToDataCallType(
                                            dataCall.type),
                                          gDataCallHelper.convertToDataCallState(
                                            dataCall.state),
                                          dataCall.name, dataCall.addresses,
                                          dataCall.gateways, dataCall.dnses);
        let webidlObj = this._window.DataCall._create(this._window, domDataCall);
        resolver.resolve(webidlObj);
        break;
      }
      case "DataCall:GetDataCallState:Resolved": {
        let state = gDataCallHelper.convertToDataCallState(data.result);
        resolver.resolve(state);
        break;
      }
      case "DataCall:RequestDataCall:Rejected":
      case "DataCall:GetDataCallState:Rejected":
        resolver.reject(data.reason);
        break;
      default:
        if (DEBUG) this.debug("Unexpected message: " + aMessage.name);
        break;
    }
  },

  /*
   * DataCallManager implementation.
   */

  requestDataCall: function(aType, aServiceId) {
    if (DEBUG) {
      this.debug("requestDataCall: " + aType + " - " + aServiceId);
    }

    return this.createPromise((aResolve, aReject) => {
      let networkType = gDataCallHelper.convertToNetworkType(aType);
      if (networkType == Ci.nsINetworkInterface.NETWORK_TYPE_UNKNOWN) {
        aReject("Invalid data call type.");
        return;
      }

      let resolverId = this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      });
      let data = { resolverId: resolverId,
                   type: networkType,
                   serviceId: aServiceId,
                   windowId: this._innerWindowId };
      cpmm.sendAsyncMessage("DataCall:RequestDataCall", data);
    });
  },

  getDataCallState: function(aType, aServiceId) {
    if (DEBUG) {
      this.debug("getDataCallState: " + aType + " - " + aServiceId);
    }

    return this.createPromise((aResolve, aReject) => {
      let networkType = gDataCallHelper.convertToNetworkType(aType);
      if (networkType == Ci.nsINetworkInterface.NETWORK_TYPE_UNKNOWN) {
        aReject("Invalid data call type.");
        return;
      }

      let resolverId = this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      });
      let data = { resolverId: resolverId,
                   type: networkType,
                   serviceId: aServiceId };
      cpmm.sendAsyncMessage("DataCall:GetDataCallState", data);
    });
  },

  /**
   * nsIObserver implementation.
   */

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
    }
  },
};

function DOMDataCall(aWindow, aWindowId, aServiceId, aType, aState, aName,
                      aAddresses, aGateways, aDnses) {
  this._id = this._generateID();

  if (DEBUG) {
    this.debug("Initializing data call - " + aType + " [" + aServiceId +
      "], name: " + aName);
  }

  this._window = aWindow;
  this._innerWindowId = aWindowId;
  this._serviceId = aServiceId;
  this.type = aType;
  this.state = aState;
  this.name = aName;
  this.addresses = aAddresses.slice();
  this.gateways = aGateways.slice();
  this.dnses = aDnses.slice();

  this.initDOMRequestHelper(this._window, DATACALL_IPC_MSG_ENTRIES);

  this._sendMessage("DataCall:Register", { windowId: this._innerWindowId });
}
DOMDataCall.prototype = {
  __proto__: DOMRequestIpcHelper.prototype,

  classDescription: "DOMDataCall",
  classID: DATACALL_CID,
  contractID: DATACALL_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIObserver]),

  _id: null,

  _window: null,

  _innerWindowId: null,

  _serviceId: 0,

  type: null,

  state: null,

  name: null,

  addresses: null,

  gateways: null,

  dnses: null,

  debug: function(aMsg) {
    dump("-*- DOMDataCall : [" + this._serviceId + "-" + this.type + "] " +
         aMsg + "\n");
  },

  _generateID: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                          .getService(Ci.nsIUUIDGenerator);
    // generateUUID() gives a UUID surrounded by {...}, slice them off.
    return uuidGenerator.generateUUID().toString().slice(1, -1);
  },

  _sendMessage: function(aMessageName, aData) {
    if (!aData) {
      aData = {};
    }

    let networkType = gDataCallHelper.convertToNetworkType(this.type);
    aData.dataCallId = this._id;
    aData.serviceId = this._serviceId;
    aData.type = networkType;

    cpmm.sendAsyncMessage(aMessageName, aData);
  },

  _isStateDisconnected: function() {
    return this.state === "disconnected";
  },

  _isStateUnavailable: function() {
    return this.state === "unavailable";
  },

  _isStateConnected: function() {
    return this.state === "connected";
  },

  _clearDataCallAttributes: function() {
    this.name = "";
    this.addresses = [];
    this.dnses = [];
    this.gateways = [];

    this._clearCachedValues();
  },

  _clearCachedValues: function() {
    this.__DOM_IMPL__._clearCachedAddressesValue();
    this.__DOM_IMPL__._clearCachedDnsesValue();
    this.__DOM_IMPL__._clearCachedGatewaysValue();
  },

  _handleStateChanged: function(aData) {
    this.state = gDataCallHelper.convertToDataCallState(aData.state,
                                                        aData.reason);

    if (this._isStateConnected()) {
      let details = aData.details;
      this.name = details.name;
      this.addresses = details.addresses;
      this.dnses = details.dnses;
      this.gateways = details.gateways;

      this._clearCachedValues();
    } else if (this._isStateUnavailable() || this._isStateDisconnected()) {
      this._clearDataCallAttributes();

      // State is unavailable, help release data call. All subsequent
      // requests to this data call are rejected.
      if (this._isStateUnavailable()) {
        if (DEBUG) this.debug("State is now unavailable.");
        this._sendMessage("DataCall:ReleaseDataCall", {});
      }
    }

    let event = new this._window.Event("statechange");
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  /*
   * Called from DOMRequestIpcHelper.
   */
  uninit: function() {
    // All requests that are still pending need to be invalidated
    // because the context is no longer valid.
    this.forEachPromiseResolver(aKey => {
      this.takePromiseResolver(aKey).reject("DataCall got destroyed.");
    });
  },

  /*
   * Message manager handler.
   */
  receiveMessage: function(aMessage) {
    let data = aMessage.data;

    // Since DataCalls of the same manager share the same target, filter out
    // messages that are not for me.
    if ((data.dataCallId && data.dataCallId != this._id) ||
        (data.dataCallIds && data.dataCallIds.indexOf(this._id) == -1)) {
      return;
    }

    if (DEBUG) {
      this.debug("Received message '" + aMessage.name + "': " +
                 JSON.stringify(aMessage.json));
    }

    let resolver;
    if (aMessage.name != "DataCall:OnStateChanged") {
      resolver = this.takePromiseResolver(data.resolverId);
      // DataCall:ReleaseDataCall may or may not have resolver.
      if (!aMessage.name.startsWith("DataCall:ReleaseDataCall") && !resolver) {
        return;
      }
    }

    switch (aMessage.name) {
      case "DataCall:ReleaseDataCall:Resolved":
      case "DataCall:ReleaseDataCall:Rejected":
        this.removeMessageListeners(DATACALL_IPC_MSG_ENTRIES);
        if (resolver) {
          resolver.resolve();
        }
        break;
      case "DataCall:AddHostRoute:Resolved":
      case "DataCall:RemoveHostRoute:Resolved":
        resolver.resolve();
        break;
      case "DataCall:AddHostRoute:Rejected":
      case "DataCall:RemoveHostRoute:Rejected":
        resolver.reject(data.reason);
        break;
      case "DataCall:OnStateChanged":
        if (this._isStateUnavailable()) {
          if (DEBUG) this.debug("Ignoring state change event on unavailable.");
          return;
        }
        this._handleStateChanged(data);
        break;
      default:
        if (DEBUG) this.debug("Unexpected message: " + aMessage.name);
        break;
    }
  },

  /*
   * DataCall implementation.
   */

  addHostRoute: function(aHost) {
    if (DEBUG) this.debug("addHostRoute: " + aHost);

    return this.createPromise((aResolve, aReject) => {
      if (this._isStateUnavailable() || this._isStateDisconnected()) {
        aReject("State is " + this.state + ".");
        return;
      }

      let resolverId = this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      });
      let data = { resolverId: resolverId,
                   host: aHost };
      this._sendMessage("DataCall:AddHostRoute", data);
    });
  },

  removeHostRoute: function(aHost) {
    if (DEBUG) this.debug("removeHostRoute: " + aHost);

    return this.createPromise((aResolve, aReject) => {
      if (this._isStateUnavailable() || this._isStateDisconnected()) {
        aReject("State is " + this.state + ".");
        return;
      }

      let resolverId = this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      });
      let data = { resolverId: resolverId,
                   host: aHost };
      this._sendMessage("DataCall:RemoveHostRoute", data);
    });
  },

  releaseDataCall: function() {
    if (DEBUG) this.debug("releaseDataCall");

    return this.createPromise((aResolve, aReject) => {
      if (this._isStateUnavailable()) {
        // This DataCall has been released already.
        aResolve();
        return;
      }

      this.state = "unavailable";
      this._clearDataCallAttributes();

      let resolverId = this.getPromiseResolverId({
        resolve: aResolve,
        reject: aReject
      });
      this._sendMessage("DataCall:ReleaseDataCall", { resolverId: resolverId });
    });
  },

  get onstatechanged() {
    return this.__DOM_IMPL__.getEventHandler("onstatechanged");
  },


  set onstatechanged(aHandler) {
    this.__DOM_IMPL__.setEventHandler("onstatechanged", aHandler);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DOMDataCallManager,
                                                     DOMDataCall]);
