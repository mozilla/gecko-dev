/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const INFO_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-info;1";
const PROVIDER_CONTRACT_ID = "@mozilla.org/presentation-device/multicastdns-provider;1";
const SD_CONTRACT_ID = "@mozilla.org/toolkit/components/mdnsresponder/dns-sd;1";
const UUID_CONTRACT_ID = "@mozilla.org/uuid-generator;1";

let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);

function MockFactory(aClass) {
  this._cls = aClass;
}
MockFactory.prototype = {
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    switch(typeof(this._cls)) {
      case "function":
        return new this._cls().QueryInterface(aIID);
      case "object":
        return this._cls.QueryInterface(aIID);
      default:
        return null;
    }
  },
  lockFactory: function(aLock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFactory])
};

function ContractHook(aContractID, aClass) {
  this._contractID = aContractID;
  this.classID = Cc[UUID_CONTRACT_ID].getService(Ci.nsIUUIDGenerator).generateUUID();
  this._newFactory = new MockFactory(aClass);

  if (!this.hookedMap.has(this._contractID)) {
    this.hookedMap.set(this._contractID, new Array());
  }

  this.init();
}

ContractHook.prototype = {
  hookedMap: new Map(), // remember only the most original factory.

  init: function() {
    this.reset();

    let oldContract = this.unregister();
    this.hookedMap.get(this._contractID).push(oldContract);
    registrar.registerFactory(this.classID, "", this._contractID, this._newFactory);

    do_register_cleanup(() => { this.cleanup.apply(this); });
  },

  reset: function() {},

  cleanup: function() {
    this.reset();

    this.unregister();
    let prevContract = this.hookedMap.get(this._contractID).pop();

    if (prevContract.factory) {
      registrar.registerFactory(prevContract.classID, "", this._contractID, prevContract.factory);
    }
  },

  unregister: function() {
    var classID, factory;

    try {
      classID = registrar.contractIDToCID(this._contractID);
      factory = Cm.getClassObject(Cc[this._contractID], Ci.nsIFactory);
    } catch (ex) {
      classID = "";
      factory = null;
    }

    if (factory) {
      registrar.unregisterFactory(classID, factory);
    }

    return { classID: classID, factory: factory };
  }
};

function MockDNSServiceInfo() {}
MockDNSServiceInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceInfo]),

  set host(aHost) {
    this._host = aHost;
  },

  get host() {
    return this._host;
  },

  set port(aPort) {
    this._port = aPort;
  },

  get port() {
    return this._port;
  },

  set serviceName(aServiceName) {
    this._serviceName = aServiceName;
  },

  get serviceName() {
    return this._serviceName;
  },

  set serviceType(aServiceType) {
    this._serviceType = aServiceType;
  },

  get serviceType() {
    return this._serviceType;
  },

  set domainName(aDomainName) {
    this._domainName = aDomainName;
  },

  get domainName() {
    return this._domainName;
  },

  set attributes(aAttributes) {
    this._attributes = aAttributes;
  },

  get attributes() {
    return this._attributes;
  }
};

function TestPresentationDeviceListener() {}
TestPresentationDeviceListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener]),

  addDevice: function(device) {},
  removeDevice: function(device) {},
  updateDevice: function(device) {}
};

function createDevice(host, port, serviceName, serviceType, domainName, attributes) {
  let device = new MockDNSServiceInfo();
  device.host = host || "";
  device.port = port || 0;
  device.serviceName = serviceName || "";
  device.serviceType = serviceType || "";
  device.domainName = domainName || "";
  device.attributes = attributes || null;
  return device;
}

function registerService() {
  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {},
    registerService: function(serviceInfo, listener) {
      this.serviceRegistered++;
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {
          this.serviceUnregistered++;
        }.bind(this)
      }
    },
    resolveService: function(serviceInfo, listener) {},
    serviceRegistered: 0,
    serviceUnregistered: 0
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);

  Assert.equal(mockObj.serviceRegistered, 0);
  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  Assert.equal(mockObj.serviceRegistered, 0);
  provider.listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener, Ci.nsISupportsWeakReference]),
    addDevice: function(device) {},
    removeDevice: function(device) {},
    updateDevice: function(device) {},
  };
  Assert.equal(mockObj.serviceRegistered, 1);

  Assert.equal(mockObj.serviceUnregistered, 0);
  provider.listener = null;
  Assert.equal(mockObj.serviceUnregistered, 1);

  run_next_test();
}

function addDevice() {
  let mockDevice = createDevice("device.local", 12345, "service.name", "_mozilla_papi._tcp");
  let mockObj = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceDiscovery]),
    startDiscovery: function(serviceType, listener) {
      listener.onDiscoveryStarted(serviceType);
      listener.onServiceFound(createDevice("", 0, mockDevice.serviceName, mockDevice.serviceType));
      return {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
        cancel: function() {}
      }
    },
    registerService: function(serviceInfo, listener) {},
    resolveService: function(serviceInfo, listener) {
      Assert.equal(serviceInfo.serviceName, mockDevice.serviceName);
      Assert.equal(serviceInfo.serviceType, mockDevice.serviceType);
      listener.onServiceResolved(createDevice(mockDevice.host, mockDevice.port, mockDevice.serviceName, mockDevice.serviceType));
    }
  };
  let contractHook = new ContractHook(SD_CONTRACT_ID, mockObj);

  let provider = Cc[PROVIDER_CONTRACT_ID].createInstance(Ci.nsIPresentationDeviceProvider);
  let listener = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDeviceListener, Ci.nsISupportsWeakReference]),
    addDevice: function(device) { this.devices.push(device); },
    removeDevice: function(device) {},
    updateDevice: function(device) {},
    devices: []
  };
  provider.listener = listener;

  Assert.equal(listener.devices.length, 0);
  provider.forceDiscovery();
  Assert.equal(listener.devices.length, 1);

  provider.listener = null;

  run_next_test();
}

function run_test() {
  let infoHook = new ContractHook(INFO_CONTRACT_ID, MockDNSServiceInfo);

  add_test(registerService);
  add_test(addDevice);

  run_next_test();
}
