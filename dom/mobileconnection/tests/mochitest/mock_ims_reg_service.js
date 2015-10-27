/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Mock ImsRegService running in chrome process.
 */

"use strict";

const {interfaces: Ci, utils: Cu, results: Cr, manager: Cm} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const IMSREGSERVICE_CONTRACTID = "@mozilla.org/mobileconnection/imsregservice;1";
const IMSREGSERVICE_CID = Components.ID("{80297610-34fa-11e5-b68f-1365a9172f05}");
const MOCK_IMSREGSERVICE_CID = Components.ID("{f3c6848b-6723-4fbd-a2ff-19f366b90b45}");

function debug(s) {
  dump("ImsRegService: " + s + "\n");
}

function ImsRegService() {
  this._handlers = [];

  // We only enable IMS in 1st slot for test.
  this._handlers.push(new ImsRegHandler(0));
}
ImsRegService.prototype = {
  classID: MOCK_IMSREGSERVICE_CID,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIImsRegService]),

  createInstance: function(aOuter, aIID) {
    if (aOuter != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },

  // An array of ImsRegHandler instances.
  _handlers: null,

  notifyCapabilityChanged: function(aCapability, aUnregisteredReason) {
    this._handlers[0].notifyCapabilityChanged(aCapability, aUnregisteredReason);
  },

  mockSetterError: function() {
    this._handlers[0]._mockSetterError = true;
  },

  /**
   * nsIImsRegService interface.
   */
  getHandlerByServiceId: function(aServiceId) {
    return this._handlers[aServiceId] || null;
  }
};

function ImsRegHandler(aServiceId) {
  this._serviceId = aServiceId;
  this._listeners = [];
}
ImsRegHandler.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIImsRegHandler]),

  _serviceId: 0,
  _listeners: null,
  _enabled: false,
  _profile: Ci.nsIImsRegHandler.IMS_PROFILE_CELLULAR_PREFERRED,
  _capability: Ci.nsIImsRegHandler.IMS_CAPABILITY_UNKNOWN,
  _unregisteredReason: null,
  _mockSetterError: false,

  _deliverListenerEvent: function(aName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (this._listeners.indexOf(listener) === -1) {
        continue;
      }
      let handler = listener[aName];
      if (typeof handler != "function") {
        throw new Error("No handler for " + aName);
      }
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        debug("listener for " + aName + " threw an exception: " + e);
      }
    }
  },

  notifyCapabilityChanged: function(aCapability, aUnregisteredReason) {
    let capabilities = [
      "voice-over-cellular",
      "voice-over-wifi",
      "video-over-cellular",
      "video-over-wifi"
    ];
    this._capability = capabilities.indexOf(aCapability);
    this._unregisteredReason = aUnregisteredReason;
    this._deliverListenerEvent("notifyCapabilityChanged",
                               [this._capability, this._unregisteredReason]);
  },

  /**
   * nsIImsRegHandler interface.
   */
  registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.push(aListener);
  },

  unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },

  getSupportedBearers: function(aCount) {
    let bearers = [
      Ci.nsIImsRegHandler.IMS_BEARER_CELLULAR,
      Ci.nsIImsRegHandler.IMS_BEARER_WIFI
    ];

    if (aCount) {
      aCount.value = bearers.length;
    }

    return bearers.slice();
  },

  setEnabled: function(aEnabled, aCallback) {
    try {
      if (this._mockSetterError) {
        throw new Error();
      }

      this._enabled = aEnabled;
      this._deliverListenerEvent("notifyEnabledStateChanged", [aEnabled]);
      aCallback.notifySuccess();
    } catch (e) {
      aCallback.notifyError("setEnabledError");
    }
  },

  get enabled() {
    return this._enabled;
  },

  setPreferredProfile: function(aProfile, aCallback) {
    try {
      if (this._mockSetterError) {
        throw new Error();
      }

      this._profile = aProfile;
      this._deliverListenerEvent("notifyPreferredProfileChanged", [aProfile]);
      aCallback.notifySuccess();
    } catch (e) {
      aCallback.notifyError("setPreferredProfileError");
    }
  },

  get preferredProfile() {
    return this._profile;
  },

  get capability() {
    return this._capability;
  },

  get unregisteredReason() {
    return this._unregisteredReason;
  },
};

let gImsRegService = new ImsRegService();

function setUpMockService() {
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(MOCK_IMSREGSERVICE_CID, "MockImsRegService",
                     IMSREGSERVICE_CONTRACTID,
                     gImsRegService);
};

function teardownMockService() {
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .registerFactory(IMSREGSERVICE_CID, null,
                     IMSREGSERVICE_CONTRACTID,
                     null); // Set to null to restore the old factory.
  Cm.QueryInterface(Ci.nsIComponentRegistrar)
    .unregisterFactory(MOCK_IMSREGSERVICE_CID, gImsRegService);
  gImsRegService = null;
};

addMessageListener("updateImsCapability", function(aMessage) {
  gImsRegService.notifyCapabilityChanged(
    aMessage.capability, aMessage.unregisteredReason);
});

addMessageListener("mockSetterError", function(aMessage) {
  gImsRegService.mockSetterError();
  sendAsyncMessage("mockSetterError-complete", null);
});

addMessageListener("setup", function(aMessage) {
  setUpMockService();
  sendAsyncMessage("setup-complete", null);
});

addMessageListener("teardown", function(aMessage) {
  teardownMockService();
});
