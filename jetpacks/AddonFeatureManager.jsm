/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const { console } = Cu.import("resource://gre/modules/devtools/Console.jsm", {});

const rootURI = "resource://gre/modules/jetpacks/";

this.EXPORTED_SYMBOLS = [ "AddonFeatureManager" ];
const { require } = Cu.import("resource://gre/modules/commonjs/toolkit/require.js", {});
const { Bootstrap } = require("resource://gre/modules/commonjs/sdk/addon/bootstrap.js");
const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const config = require(rootURI + "config.json");
const { addons } = config;
const ADDONS_LENGTH = addons.length;

const REASON = [ "unknown", "startup", "shutdown", "enable", "disable",
                 "install", "uninstall", "upgrade", "downgrade" ];

function getIndexById(id) {
  for (let i = ADDONS_LENGTH - 1; i >= 0; i--) {
    if (addons[i].id == id) {
      return i;
    }
  }
  return null;
}

var started = false;

this.AddonFeatureManager = {
  _boostraps: new Array(ADDONS_LENGTH),
  startup: function() {
    if (started) return;
    started = true;
    console.log("AFM: Startup");

    // Enable all default add-on features
    this._boostraps = addons.map(addon => {
      let bootstrap = new Bootstrap(addon.rootURI);
      console.log("AFM: Startup " + addon.manifest.id);
      bootstrap.startup(createAddonObject(addon), REASON.indexOf("startup"));
    });
  },
  disable: function disableAddonFeature({ id }) {
    console.log("AFM: Disable " + id);

    let bootstraps = this._bootstraps;
    let i = getIndexById(id);
    if (!i) return null;
    console.log("AFM: Disable " + id);
    let promise = bootstrap.shutdown(createAddonObject(addons[i]), REASON.indexOf("disable"));
    bootstraps[i] = null;
  },
  enable: function enableAddonFeature({ id }) {
    console.log("AFM: Enable " + id);

    let bootstraps = this._bootstraps;
    let i = getIndexById(id);
    if (!i) return null;
    console.log("AFM: Enable " + id);
    let bootstrap = new Bootstrap(addons[i].rootURI);
    bootstrap.startup(createAddonObject(addons[i]), REASON.indexOf("enable"));
    bootstraps[i] = bootstrap;
  },
  shutdown: function() {
    console.log("AFM: Shutdown");

    let bootstraps = this._bootstraps;
    this._bootstraps = new Array(ADDONS_LENGTH);
    addons.forEach((addon, i) => {
      let bootstrap = this._bootstraps[i];
      if (!bootstrap) return null;
      console.log("AFM: Shutdown " + addon.manifest.id);
      bootstrap.shutdown(createAddonObject(addon), REASON.indexOf("shutdown"));
    });
  }
};

function createAddonObject({ manifest, rootURI }) {
  return {
    id: manifest.id,
    version: manifest.version,
    resourceURI: {
      spec: rootURI
    }
  };
}

var AddonManagerListener = {
  "onStartup": function() {
    AddonFeatureManager.startup();
  },
  "onShutdown": function() {
    AddonFeatureManager.shutdown();
  },
  "onEnabling": function(addon, needsRestart) {
    AddonFeatureManager.disable(addon);
  },
  "onInstalling": function(addon, needsRestart) {
    AddonFeatureManager.disable(addon);
  },
  "onDisabling": function(addon, needsRestart) {
    AddonFeatureManager.enable(addon);
  },
  "onUninstall": function(addon, needsRestart) {
    AddonFeatureManager.enable(addon);
  }
};
AddonManager.addManagerListener(AddonManagerListener);
