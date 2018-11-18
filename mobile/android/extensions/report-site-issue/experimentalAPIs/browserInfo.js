/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const gParentalControls = (function() {
  if ("@mozilla.org/parental-controls-service;1" in Cc) {
    return Cc["@mozilla.org/parental-controls-service;1"]
           .createInstance(Ci.nsIParentalControlsService);
  }
  return {parentalControlsEnabled: false};
})();

this.browserInfo = class extends ExtensionAPI {
  getAPI(context) {
    return {
      browserInfo: {
        async getGraphicsPrefs() {
          const prefs = {};
          for (const [name, dflt] of Object.entries({
            "layers.acceleration.force-enabled": false,
            "gfx.webrender.all": false,
            "gfx.webrender.blob-images": true,
            "gfx.webrender.enabled": false,
            "image.mem.shared": true,
          })) {
            prefs[name] = Services.prefs.getBoolPref(name, dflt);
          }
          return prefs;
        },
        async getAppVersion() {
          return AppConstants.MOZ_APP_VERSION;
        },
        async getBlockList() {
          const trackingTable = Services.prefs.getCharPref("urlclassifier.trackingTable");
          // If content-track-digest256 is in the tracking table,
          // the user has enabled the strict list.
          return trackingTable.includes("content") ? "strict" : "basic";
        },
        async getBuildID() {
          return Services.appinfo.appBuildID;
        },
        async getUpdateChannel() {
          return AppConstants.MOZ_UPDATE_CHANNEL;
        },
        async getParentalControlsEnabled() {
          return gParentalControls.parentalControlsEnabled;
        },
        async getPlatform() {
          return AppConstants.platform;
        },
        async hasTouchScreen() {
          const gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
          return gfxInfo.getInfo().ApzTouchInput == 1;
        },
      },
    };
  }
};
