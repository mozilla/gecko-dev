/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const DATACALLSERVICE_CONTRACTID = "@mozilla.org/datacallservice;1;"
const DATACALLSERVICE_CID        = Components.ID("{e29c041d-290d-4e6c-8bca-452f6557de68}");

const DATACALL_IPC_MSG_ENTRIES = [
  "DataCall:RequestDataCall",
  "DataCall:ReleaseDataCall",
  "DataCall:GetDataCallState",
  "DataCall:AddHostRoute",
  "DataCall:RemoveHostRoute",
  "DataCall:Register",
];

const DATACALL_CONNECT_TIMEOUT = 30000;
const DATACALL_TYPES = [
  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_MMS,
  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_SUPL,
  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_IMS,
  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_DUN,
  Ci.nsINetworkInterface.NETWORK_TYPE_MOBILE_FOTA
];

const NETWORK_STATE_UNKNOWN       = Ci.nsINetworkInterface.NETWORK_STATE_UNKNOWN;
const NETWORK_STATE_CONNECTING    = Ci.nsINetworkInterface.NETWORK_STATE_CONNECTING;
const NETWORK_STATE_CONNECTED     = Ci.nsINetworkInterface.NETWORK_STATE_CONNECTED;
const NETWORK_STATE_DISCONNECTING = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTING;
const NETWORK_STATE_DISCONNECTED  = Ci.nsINetworkInterface.NETWORK_STATE_DISCONNECTED;

const TOPIC_XPCOM_SHUTDOWN             = "xpcom-shutdown";
const TOPIC_INNER_WINDOW_DESTROYED     = "inner-window-destroyed";
const TOPIC_CONNECTION_STATE_CHANGED   = "network-connection-state-changed";
const MESSAGE_CHILD_PROCESS_SHUTDOWN   = "child-process-shutdown";
const SETTINGS_DATA_DEFAULT_SERVICE_ID = "ril.data.defaultServiceId";
const PREF_RIL_DEBUG_ENABLED           = "ril.debugging.enabled";

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "gRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIMobileConnectionService");

/* global RIL */
XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
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

function DataCallService() {
  if (DEBUG) this.debug("DataCallService starting.");

  this.dataCallsContext = {};
  for (let type of DATACALL_TYPES) {
    this.dataCallsContext[type] = {};
    this.dataCallsContext[type].connectTimer = null;
    this.dataCallsContext[type].requestTargets = [];
  }

  this._dataDefaultServiceId = 0;
  this._listeners = {};

  // Read the default service id for data call.
  gSettingsService.createLock().get(SETTINGS_DATA_DEFAULT_SERVICE_ID, this);

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
  Services.obs.addObserver(this, TOPIC_INNER_WINDOW_DESTROYED, false);
  Services.obs.addObserver(this, TOPIC_CONNECTION_STATE_CHANGED, false);

  this._registerMessageListeners();
}
DataCallService.prototype = {
  classDescription: "DataCallService",
  classID: DATACALLSERVICE_CID,
  contractID: DATACALLSERVICE_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  dataCallsContext: null,

  _dataDefaultServiceId: null,

  _listeners: null,

  debug: function(aMsg) {
    dump("-*- DataCallService: " + aMsg + "\n");
  },

  _registerMessageListeners: function() {
    ppmm.addMessageListener(MESSAGE_CHILD_PROCESS_SHUTDOWN, this);
    for (let msgName of DATACALL_IPC_MSG_ENTRIES) {
      ppmm.addMessageListener(msgName, this);
    }
  },

  _unregisterMessageListeners: function() {
    ppmm.removeMessageListener(MESSAGE_CHILD_PROCESS_SHUTDOWN, this);
    for (let msgName of DATACALL_IPC_MSG_ENTRIES) {
      ppmm.removeMessageListener(msgName, this);
    }
    ppmm = null;
  },

  _createDataCall: function(aNetwork) {
    let ips = {};
    let prefixLengths = {};
    let length = aNetwork.getAddresses(ips, prefixLengths);

    let addresses = [];
    for (let i = 0; i < length; i++) {
      let addr = ips.value[i] + "/" + prefixLengths.value[i];
      addresses.push(addr);
    }

    let dataCall = {
      state: aNetwork.state,
      serviceId: aNetwork.serviceId,
      type: aNetwork.type,
      name: aNetwork.name,
      addresses: addresses,
      gateways: aNetwork.getGateways(),
      dnses: aNetwork.getDnses()
    };

    return dataCall;
  },

  getDataCallContext: function(aType) {
    return this.dataCallsContext[aType];
  },

  _processContextRequests: function(aDataCallContext, aResult) {
    if (!aDataCallContext.requestTargets.length) {
      return;
    }

    aDataCallContext.requestTargets.forEach(aRequest => {
      if (aResult) {
        let target = aRequest.target;
        let resolverId = aRequest.resolverId;

        if (aResult.errorMsg) {
          target.sendAsyncMessage("DataCall:RequestDataCall:Rejected",
                                  {resolverId: resolverId,
                                   reason: aResult.errorMsg});
        } else {
          target.sendAsyncMessage("DataCall:RequestDataCall:Resolved",
                                  {resolverId: resolverId,
                                   result: aResult});
        }
      }
    });
    aDataCallContext.requestTargets = [];
  },

  _cleanupRequestsByTarget: function(aTarget) {
    for (let type of DATACALL_TYPES) {
      let context = this.dataCallsContext[type];
      let requests = context.requestTargets;

      for (let i = requests.length - 1; i >= 0; i--) {
        if (requests[i].target == aTarget) {
          this._deactivateDataCall(requests[i].serviceId, requests[i].type);
          requests.splice(i, 1);
        }
      }

      if (requests.length == 0 && context.connectTimer) {
        context.connectTimer.cancel();
        context.connectTimer = null;
      }
    }
  },

  _cleanupRequestsByWinId: function(aWindowId) {
    for (let type of DATACALL_TYPES) {
      let context = this.dataCallsContext[type];
      let requests = context.requestTargets;

      for (let i = requests.length - 1; i >= 0; i--) {
        if (requests[i].windowId == aWindowId) {
          this._deactivateDataCall(requests[i].serviceId, requests[i].type);
          requests.splice(i, 1);
        }
      }

      if (requests.length == 0 && context.connectTimer) {
        context.connectTimer.cancel();
        context.connectTimer = null;
      }
    }
  },

  onConnectionStateChanged: function(aNetwork) {
    let context = this.getDataCallContext(aNetwork.type);
    if (!context) {
      return;
    }

    if (aNetwork.state == NETWORK_STATE_CONNECTED &&
        context.requestTargets.length) {
      context.connectTimer.cancel();
      context.connectTimer = null;

      let dataCall = this._createDataCall(aNetwork);
      this._processContextRequests(context, dataCall);
    }
  },

  sendStateChangeEvent: function(aNetwork) {
    let topic = this._getTopicName(aNetwork.serviceId, aNetwork.type);

    let details = {};
    if (aNetwork.state == NETWORK_STATE_CONNECTED) {
      let dataCall = this._createDataCall(aNetwork);
      details.name = dataCall.name;
      details.addresses = dataCall.addresses;
      details.gateways = dataCall.gateways;
      details.dnses = dataCall.dnses;
    }

    this._notifyMessageTarget(topic, "DataCall:OnStateChanged", {
      state: aNetwork.state,
      reason: aNetwork.reason,
      details: details
    });
  },

  _getNetworkInterface: function(aServiceId, aType) {
    for each (let network in gNetworkManager.networkInterfaces) {
      if (network.type == aType) {
        try {
          if (network instanceof Ci.nsIRilNetworkInterface) {
            let rilNetwork = network.QueryInterface(Ci.nsIRilNetworkInterface);
            if (rilNetwork.serviceId != aServiceId) {
              continue;
            }
          }
        } catch (e) {}
        return network;
      }
    }
    return null;
  },

  _sendResponse: function(aMessage, aResult) {
    if (aResult && aResult.errorMsg) {
      aMessage.target.sendAsyncMessage(aMessage.name + ":Rejected",
                                       { dataCallId: aMessage.data.dataCallId,
                                         resolverId: aMessage.data.resolverId,
                                         reason: aResult.errorMsg});
      return;
    }

    aMessage.target.sendAsyncMessage(aMessage.name + ":Resolved",
                                     { dataCallId: aMessage.data.dataCallId,
                                       resolverId: aMessage.data.resolverId,
                                       result: aResult});
  },

  _setupDataCall: function(aData, aTargetCallback, aTarget) {
    if (DEBUG) {
      this.debug("Setup data call for " + aData.serviceId + "-" + aData.type);
    }

    let serviceId = (aData.serviceId != undefined ? aData.serviceId :
      this._dataDefaultServiceId);
    let type = aData.type;

    let context = this.getDataCallContext(type);
    if (!context) {
      aTargetCallback({ errorMsg: "Mobile network type not supported." });
      return;
    }

    let ril = gRil.getRadioInterface(serviceId);
    if (!ril) {
      aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      return;
    }

    let connection =
      gMobileConnectionService.getItemByServiceId(this._dataDefaultServiceId);
    if (connection.radioState != Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED) {
      aTargetCallback({ errorMsg: "Radio state is off." });
      return;
    }

    let dataInfo = connection && connection.data;
    if (dataInfo && dataInfo.state != "registered") {
      aTargetCallback({ errorMsg: "Data registration not registered." });
      return;
    }

    try {
      // This indicates there is no network interface for this mobile type.
      if (ril.getDataCallStateByType(type) == NETWORK_STATE_UNKNOWN) {
        aTargetCallback({ errorMsg: "Network interface not available for type." });
        return;
      }
    } catch (e) {
      // Throws when type is not a mobile network type.
      aTargetCallback({ errorMsg: "Error setting up data call: " + e });
      return;
    }

    // Call this no matter what for ref counting in RadioInterfaceLayer.
    ril.setupDataCallByType(type);

    if (ril.getDataCallStateByType(type) == NETWORK_STATE_CONNECTED) {
      let networkIface = this._getNetworkInterface(serviceId, type);
      let dataCall = this._createDataCall(networkIface);
      aTargetCallback(dataCall);
      return;
    }

    context.requestTargets.push({ target: aTarget,
                                  windowId: aData.windowId,
                                  resolverId: aData.resolverId,
                                  serviceId: serviceId,
                                  type: type });

    // Start connect timer if not started yet.
    if (context.connectTimer) {
      return;
    }
    context.connectTimer =
      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    context.connectTimer.initWithCallback(() => {
      context.connectTimer = null;
      if (DEBUG) {
        this.debug("Connection time out for type: " + type);
      }
      this._processContextRequests(context, { errorMsg: "Connection timeout."});
    }, DATACALL_CONNECT_TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _getDataCallState: function(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Get data call state for: " + aData.serviceId + "-" +
                 aData.type);
    }

    let serviceId = (aData.serviceId != undefined ? aData.serviceId :
      this._dataDefaultServiceId);
    let ril = gRil.getRadioInterface(serviceId);
    if (!ril) {
      aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      return;
    }

    let state;
    try {
      state = ril.getDataCallStateByType(aData.type);
    } catch (e) {
      // Throws when type is not a mobile network type.
      aTargetCallback({ errorMsg: "Error getting data call state: " + e });
      return;
    }

    aTargetCallback(state);
  },

  _releaseDataCall: function(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Release data call: " + + aData.serviceId + "-" + aData.type);
    }

    let type = aData.type;
    let serviceId = aData.serviceId;

    this._deactivateDataCall(serviceId, type, aTargetCallback);
  },

  _addHostRoute: function(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Add host route for " + aData.serviceId + "-" + aData.type);
    }

    let networkIface = this._getNetworkInterface(aData.serviceId, aData.type);
    gNetworkManager.addHostRoute(networkIface, aData.host)
      .then(() => {
        aTargetCallback();
      }, aReason => {
        aTargetCallback({ errorMsg: aReason });
      });
  },

  _removeHostRoute: function(aData, aTargetCallback) {
    if (DEBUG) {
      this.debug("Remove host route for " + aData.serviceId + "-" + aData.type);
    }

    let networkIface = this._getNetworkInterface(aData.serviceId, aData.type);
    gNetworkManager.removeHostRoute(networkIface, aData.host)
      .then(() => {
        aTargetCallback();
      }, aReason => {
        aTargetCallback({ errorMsg: aReason });
      });
  },

  _deactivateDataCall: function(aServiceId, aType, aTargetCallback) {
    let ril = gRil.getRadioInterface(aServiceId);
    if (!ril) {
      if (aTargetCallback) {
        aTargetCallback({ errorMsg: "Can not get valid radio interface." });
      }
      return;
    }

    try {
      // Call this no matter what for ref counting in RadioInterfaceLayer.
      ril.deactivateDataCallByType(aType);
    } catch (e) {
      // Throws when type is not a mobile network type.
      if (aTargetCallback) {
        aTargetCallback({ errorMsg: "Error deactivating data call: " + e });
      }
      return;
    }

    if (aTargetCallback) {
      aTargetCallback();
    }
  },

  _registerMessageTarget: function(aTopic, aTarget, aData) {
    let listeners = this._listeners[aTopic];
    if (!listeners) {
      listeners = this._listeners[aTopic] = [];
    }

    listeners.push({ target: aTarget,
                     windowId: aData.windowId,
                     dataCallId: aData.dataCallId });
    if (DEBUG) {
      this.debug("Registerd " + aTopic + " dataCallId: " + aData.dataCallId);
    }
  },

  _unregisterMessageTarget: function(aTopic, aTarget, aDataCallId, aCleanup) {
    if (!aTopic) {
      for (let topic in this._listeners) {
        this._unregisterMessageTarget(topic, aTarget, aDataCallId, aCleanup);
      }
      return;
    }

    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    let index = listeners.length;
    while (index--) {
      let listener = listeners[index];
      // In addition to target, dataCallId must match, if available.
      if (listener.target === aTarget &&
          (!aDataCallId || listener.dataCallId === aDataCallId)) {

        if (DEBUG) {
          this.debug("Unregistering " + aTopic + " dataCallId: " +
                     listener.dataCallId);
        }

        listeners.splice(index, 1);
        // Cleanup data call if it was not released propoerly.
        if (aCleanup) {
          let [serviceId, type] = aTopic.split("-");
          this._deactivateDataCall(serviceId, type);
        }
      }
    }
  },

  _unregisterMessageTargetByWinId: function(aTopic, aWindowId, aCleanup) {
    if (!aTopic) {
      for (let topic in this._listeners) {
        this._unregisterMessageTargetByWinId(topic, aWindowId, aCleanup);
      }
      return;
    }

    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    let index = listeners.length;
    while (index--) {
      if (listeners[index].windowId === aWindowId) {
        if (DEBUG) {
          this.debug("Unregistering " + aTopic + " windowId: " + aWindowId);
        }

        listeners.splice(index, 1);
        // Cleanup data call if it was not released propoerly.
        if (aCleanup) {
          let [serviceId, type] = aTopic.split("-");
          this._deactivateDataCall(serviceId, type);
        }
      }
    }
  },

  _notifyMessageTarget: function(aTopic, aMessage, aOptions) {
    let listeners = this._listeners[aTopic];
    if (!listeners) {
      return;
    }

    // Group listeners that have the same target to avoid flooding the IPC
    // channel.
    let groupedListeners = new Map();
    listeners.forEach(aListener => {
      let target = aListener.target;
      if (!groupedListeners.has(target)) {
        groupedListeners.set(target, [aListener.dataCallId]);
      } else {
        let ids = groupedListeners.get(target);
        ids.push(aListener.dataCallId);
        groupedListeners.set(target, ids);
      }
    });

    groupedListeners.forEach((ids, target) => {
      aOptions.dataCallIds = ids;
      target.sendAsyncMessage(aMessage, aOptions);
    });
  },

  _getTopicName: function(aServiceId, aType) {
    return aServiceId + "-" + aType;
  },

  /**
   * nsIObserver interface methods.
   */

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_CONNECTION_STATE_CHANGED:
        if (!(aSubject instanceof Ci.nsIRilNetworkInterface)) {
          return;
        }
        let network = aSubject.QueryInterface(Ci.nsIRilNetworkInterface);
        if (DEBUG) {
          this.debug("Network " + network.type + "/" + network.name +
                     " changed state to " + network.state);
        }
        this.onConnectionStateChanged(network);
        this.sendStateChangeEvent(network);
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        for (let type of DATACALL_TYPES) {
          let context = this.dataCallsContext[type];
          if (context.connectTimer) {
            context.connectTimer.cancel();
            context.connectTimer = null;
          }
          this._processContextRequests(context, { errorMsg: "xpcom-shutdown" });
          delete this.dataCallsContext[type];
        }
        this.dataCallsContext = null;

        this._unregisterMessageListeners();
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        Services.obs.removeObserver(this, TOPIC_INNER_WINDOW_DESTROYED);
        Services.obs.removeObserver(this, TOPIC_CONNECTION_STATE_CHANGED);
        break;
      case TOPIC_INNER_WINDOW_DESTROYED:
        let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
        this._cleanupRequestsByTarget(wId);
        this._unregisterMessageTargetByWinId(null, wId, true);
        break;
    }
  },

  /**
   * nsISettingsServiceCallback interface methods.
   */

  handle: function(aName, aResult) {
    switch (aName) {
      case SETTINGS_DATA_DEFAULT_SERVICE_ID:
        this._dataDefaultServiceId = aResult || 0;
        if (DEBUG) {
          this.debug("'_dataDefaultServiceId' is now " +
                     this._dataDefaultServiceId);
        }
        break;
    }
  },

  /**
   * nsIMessageListener interface methods.
   */

  receiveMessage: function(aMessage) {
    if (DEBUG) {
      this.debug("Received '" + aMessage.name + "' message from content process.");
    }

    if (aMessage.name == MESSAGE_CHILD_PROCESS_SHUTDOWN) {
      this._cleanupRequestsByTarget(aMessage.target);
      this._unregisterMessageTarget(null, aMessage.target, null, true);
      return;
    }

    if (DATACALL_IPC_MSG_ENTRIES.indexOf(aMessage.name) !== -1) {
      if (!aMessage.target.assertPermission("datacall")) {
        if (DEBUG) {
          this.debug("DataCall message " + aMessage.name +
                     " from a content process with no 'datacall' privileges.");
        }
        return;
      }
    } else {
      if (DEBUG) this.debug("Ignoring unknown message type: " + aMessage.name);
      return;
    }

    let callback = (aResult) => this._sendResponse(aMessage, aResult);
    let data = aMessage.data;

    switch (aMessage.name) {
      case "DataCall:RequestDataCall":
        this._setupDataCall(data, callback, aMessage.target);
        break;
      case "DataCall:GetDataCallState":
        this._getDataCallState(data, callback);
        break;
      case "DataCall:ReleaseDataCall": {
        let topic = this._getTopicName(data.serviceId, data.type);
        this._unregisterMessageTarget(topic, aMessage.target, data.dataCallId);
        this._releaseDataCall(data, callback);
        break;
      }
      case "DataCall:AddHostRoute":
        this._addHostRoute(data, callback);
        break;
      case "DataCall:RemoveHostRoute":
        this._removeHostRoute(data, callback);
        break;
      case "DataCall:Register": {
        let topic = this._getTopicName(data.serviceId, data.type);
        this._registerMessageTarget(topic, aMessage.target, data);
        break;
      }
    }

    return null;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataCallService]);
