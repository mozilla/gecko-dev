/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const Ci = Components.interfaces;
const Cc = Components.classes;

const POSITION_UNAVAILABLE = Ci.nsIDOMGeoPositionError.POSITION_UNAVAILABLE;

let gLoggingEnabled = false;

// Poll every 6 seconds. Note that this value must be slightly longer
// than the wifi scanning interval, to maximize the likelihood that
// we'll have wifi results when we send our position request. If this
// value is 5000, then we often report cell-only locations and ignore
// the wifi data.
// XXX This is horrible, brittle hack, but it seems to work on the Tarako.
let gTimeToWaitBeforeSending = 6000; //ms

let gWifiScanningEnabled = true;
let gWifiResults;

let gCellScanningEnabled = false;

function LOG(aMsg) {
  if (gLoggingEnabled) {
    aMsg = "*** WIFI GEO: " + aMsg + "\n";
    Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).logStringMessage(aMsg);
    dump(aMsg);
  }
}

function CachedLocation(loc, cellInfo, wifiList) {
  this.location = loc;
  this.cellInfo = cellInfo;
  this.wifiList = wifiList;
}

CachedLocation.prototype = {
  isGeoip: function() {
    return !this.cellInfo && !this.wifiList;
  },
  isCellAndWifi: function() {
    return this.cellInfo && this.wifiList;
  },
  isCellOnly: function() {
    return this.cellInfo && !this.wifiList;
  },
  isWifiOnly: function() {
    return this.wifiList && !this.cellInfo;
  },
  // if 50% of the SSIDS match
  isWifiApproxEqual: function(wifiList) {
    if (!this.wifiList) {
      return false;
    }

    // if either list is a 50% subset of the other, they are equal
    let myLen = this.wifiList.length;
    let otherLen = wifiList.length;

    let largerList = this.wifiList;
    let smallerList = wifiList;
    if (myLen < otherLen) {
      largerList = wifiList;
      smallerList = this.wifiList;
    }

    let wifiHash = {};
    for (let i = 0; i < largerList.length; i++) {
      wifiHash[largerList[i].macAddress] = 1;
    }

    let common = 0;
    for (let i = 0; i < smallerList.length; i++) {
      if (smallerList[i].macAddress in wifiHash) {
        common++;
      }
    }
    let kPercentMatch = 0.5;
    return common >= (largerList.length * kPercentMatch);
  },
  // if fields match
  isCellApproxEqual: function(cellInfo) {
    if (!this.cellInfo) {
      return false;
    }

    let len1 = this.cellInfo.length;
    let len2 = cellInfo.length;

    if (len1 != len2) {
      LOG("cell not equal len");
      return false;
    }

    // Use only these values for equality
    // (the JSON will contain additional values in future)
    function makeCellHashKey(cell) {
      return "" + cell.radio + ":" + cell.mobileCountryCode + ":" +
             cell.mobileNetworkCode + ":" + cell.locationAreaCode + ":" +
             cell.cellId;
    }

    let cellHash = {};
    for (let i = 0; i < len1; i++) {
      cellHash[makeCellHashKey(this.cellInfo[i])] = 1;
    }

    for (let i = 0; i < len2; i++) {
      if (!(makeCellHashKey(cellInfo[i]) in cellHash)) {
        return false;
      }
    }

    return true;
  }
};

let gCachedLocation = null;

// This function serves two purposes:
// 1) do we have a cached location
// 2) is the cached location better than what newCell and newWifiList will obtain
// If the cached location exists, and we know it to have greater accuracy
// by the nature of its origin (wifi/cell/geoip), use the cached location.
// 
// If there is more source info than the cachedLocation had, return false
// In other cases, MLS is known to produce better/worse accuracy based on the 
// inputs, so base the decision on that.
function isCachedLocationMoreAccurateThanServerRequest(newCell, newWifiList)
{
  if (!gCachedLocation) {
    return false;
  }

  // if new request has both cell and wifi, and old is just cell,
  if (gCachedLocation.isCellOnly() && newCell && newWifiList) {
    return false;
  }

  // if new is geoip request
  if (!newCell && !newWifiList) {
    return true;
  }

  let hasEqualCells = false;
  if (newCell) {
    hasEqualCells = gCachedLocation.isCellApproxEqual(newCell);
  }

  let hasEqualWifis = false;
  if (newWifiList) {
    hasEqualWifis = gCachedLocation.isWifiApproxEqual(newWifiList);
  }

  if (gCachedLocation.isCellOnly()) {
    if (hasEqualCells) {
      return true;
    }
  } else if (gCachedLocation.isWifiOnly() && hasEqualWifis) {
    return true;
  } else if (gCachedLocation.isCellAndWifi()) {
    if ((hasEqualCells && hasEqualWifis) ||
         (!newWifiList && hasEqualCells) ||
          (!newCell && hasEqualWifis))
    {
      return true;
    }
  }

  return false;
};

function WifiGeoCoordsObject(lat, lon, acc, alt, altacc) {
  this.latitude = lat;
  this.longitude = lon;
  this.accuracy = acc;
  this.altitude = alt;
  this.altitudeAccuracy = altacc;
}

WifiGeoCoordsObject.prototype = {
  QueryInterface:  XPCOMUtils.generateQI([Ci.nsIDOMGeoPositionCoords]),

  classInfo: XPCOMUtils.generateCI({interfaces: [Ci.nsIDOMGeoPositionCoords],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "wifi geo position coords object"}),
};

function WifiGeoPositionObject(lat, lng, acc) {
  this.coords = new WifiGeoCoordsObject(lat, lng, acc, 0, 0);
  this.address = null;
  this.timestamp = Date.now();
}

WifiGeoPositionObject.prototype = {
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIDOMGeoPosition]),

  // Class Info is required to be able to pass objects back into the DOM.
  classInfo: XPCOMUtils.generateCI({interfaces: [Ci.nsIDOMGeoPosition],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "wifi geo location position object"}),
};

function WifiGeoPositionProvider() {
  try {
    gLoggingEnabled = Services.prefs.getBoolPref("geo.wifi.logging.enabled");
  } catch (e) {}

  try {
    gTimeToWaitBeforeSending = Services.prefs.getIntPref("geo.wifi.timeToWaitBeforeSending");
  } catch (e) {}

  try {
    gWifiScanningEnabled = Services.prefs.getBoolPref("geo.wifi.scan");
  } catch (e) {}

  try {
    gCellScanningEnabled = Services.prefs.getBoolPref("geo.cell.scan");
  } catch (e) {}

  this.wifiService = null;
  this.timeoutTimer = null;
  this.started = false;
}

WifiGeoPositionProvider.prototype = {
  classID:          Components.ID("{77DA64D3-7458-4920-9491-86CC9914F904}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIGeolocationProvider,
                                           Ci.nsIWifiListener,
                                           Ci.nsITimerCallback]),
  listener: null,
    
  startup:  function() {
    if (this.started)
      return;
    this.started = true;

    if (gWifiScanningEnabled) {
      this.wifiService = Cc["@mozilla.org/wifi/monitor;1"].getService(Components.interfaces.nsIWifiMonitor);
      this.wifiService.startWatching(this);
    }
    this.timeoutTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.timeoutTimer.initWithCallback(this,
                                       gTimeToWaitBeforeSending,
                                       this.timeoutTimer.TYPE_REPEATING_SLACK);
    LOG("startup called.");
  },

  watch: function(c) {
    this.listener = c;
  },

  shutdown: function() {
    LOG("shutdown called");
    if (this.started == false) {
      return;
    }

    // Without clearing this, we could endup using the cache almost indefinitely
    // TODO: add logic for cache lifespan, for now just be safe and clear it
    gCachedLocation = null;
    gWifiResults = null;

    if (this.timeoutTimer) {
      this.timeoutTimer.cancel();
      this.timeoutTimer = null;
    }

    if(this.wifiService) {
      this.wifiService.stopWatching(this);
      this.wifiService = null;
    }
    this.listener = null;
    this.started = false;
  },

  setHighAccuracy: function(enable) {
  },

  onChange: function(accessPoints) {

    function isPublic(ap) {
      let mask = "_nomap"
      let result = ap.ssid.indexOf(mask, ap.ssid.length - mask.length);
      if (result != -1) {
        LOG("Filtering out " + ap.ssid + " " + result);
      }
      return result;
    };

    function sort(a, b) {
      return b.signal - a.signal;
    };

    function encode(ap) {
      return { 'macAddress': ap.mac, 'signalStrength': ap.signal };
    };

    if (accessPoints) {
      gWifiResults = accessPoints.filter(isPublic).sort(sort).map(encode);
    } else {
      gWifiResults = null;
    }
  },

  onError: function (code) {
    LOG("wifi error: " + code);
  },

  updateMobileInfo: function() {
    LOG("updateMobileInfo called");
    try {
      let radioService = Cc["@mozilla.org/ril;1"]
                    .getService(Ci.nsIRadioInterfaceLayer);
      let numInterfaces = radioService.numRadioInterfaces;
      let result = [];
      for (let i = 0; i < numInterfaces; i++) {
        LOG("Looking for SIM in slot:" + i + " of " + numInterfaces);
        let radio = radioService.getRadioInterface(i);
        let iccInfo = radio.rilContext.iccInfo;
        let cell = radio.rilContext.voice.cell;

        if (iccInfo && cell) {
          // TODO type and signal strength
          result.push({ radio: "gsm",
                      mobileCountryCode: iccInfo.mcc,
                      mobileNetworkCode: iccInfo.mnc,
                      locationAreaCode: cell.gsmLocationAreaCode,
                      cellId: cell.gsmCellId });
        }
      }
      return result;
    } catch (e) {
      return null;
    }
  },

  notify: function (timeoutTimer) {
    let cellResults = null;
    if (gCellScanningEnabled) {
      cellResults = this.updateMobileInfo();
    }

    let data = {};
    if (cellResults) {
      data.cellTowers = cellResults;
    }
    if (gWifiResults) {
      data.wifiAccessPoints = gWifiResults;
    }

    let useCached = isCachedLocationMoreAccurateThanServerRequest(data.cellTowers,
                                                                  data.wifiAccessPoints);
    if (useCached) {
      this.listener.update(gCachedLocation.location);
      return;
    }

    // From here on, do a network request for location //
    let url = Services.urlFormatter.formatURLPref("geo.wifi.uri");
    let listener = this.listener;

    let xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                        .createInstance(Ci.nsIXMLHttpRequest);

    try {
      xhr.open("POST", url, true);
    } catch (e) {
      listener.notifyError(POSITION_UNAVAILABLE);
      return;
    }

    xhr.setRequestHeader("Content-Type", "application/json; charset=UTF-8");
    xhr.responseType = "json";
    xhr.mozBackgroundRequest = true;
    xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS;
    xhr.onerror = function() {
      listener.notifyError(POSITION_UNAVAILABLE);
    };
    xhr.onload = function() {
      LOG("gls returned status: " + xhr.status + " --> " +  JSON.stringify(xhr.response));
      if ((xhr.channel instanceof Ci.nsIHttpChannel && xhr.status != 200) ||
          !xhr.response || !xhr.response.location) {
        listener.notifyError(POSITION_UNAVAILABLE);
        return;
      }

      let newLocation = new WifiGeoPositionObject(xhr.response.location.lat,
                                                  xhr.response.location.lng,
                                                  xhr.response.accuracy);

      listener.update(newLocation);
      gCachedLocation = new CachedLocation(newLocation, data.cellTowers, data.wifiAccessPoints);
    };

    jsonData = JSON.stringify(data);
    LOG("sending " + jsonData);
    xhr.send(jsonData);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([WifiGeoPositionProvider]);
