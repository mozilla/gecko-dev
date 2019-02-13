/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["WebappManager"];

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Webapps.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/NativeApp.jsm");
Cu.import("resource://gre/modules/WebappOSUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://webapprt/modules/WebappRT.jsm");


this.WebappManager = {
  observe: function(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);
    data.mm = aSubject;

    let chromeWin;
    switch (aTopic) {
      case "webapps-ask-install":
        chromeWin = Services.wm.getOuterWindowWithId(data.oid);
        if (chromeWin)
          this.doInstall(data, chromeWin);
        break;
      case "webapps-ask-uninstall":
        chromeWin = Services.wm.getOuterWindowWithId(data.windowId);
        if (chromeWin) {
          this.doUninstall(data, chromeWin);
        }
        break;
      case "webapps-launch":
        WebappOSUtils.launch(data);
        break;
      case "webapps-uninstall":
        WebappOSUtils.uninstall(data).then(null, Cu.reportError);
        break;
    }
  },

  update: function(aApp, aManifest, aZipPath) {
    let nativeApp = new NativeApp(aApp, aManifest,
                                  WebappRT.config.app.categories,
                                  WebappRT.config.registryDir);
    nativeApp.prepareUpdate(aApp, aManifest, aZipPath);
  },

  doInstall: function(data, window) {
    let jsonManifest = data.isPackage ? data.app.updateManifest : data.app.manifest;
    let manifest =
      new ManifestHelper(jsonManifest, data.app.origin, data.app.manifestURL);
    let name = manifest.name;
    let bundle = Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");

    let choice = Services.prompt.confirmEx(
      window,
      bundle.formatStringFromName("webapps.install.title", [name], 1),
      bundle.formatStringFromName("webapps.install.description", [name], 1),
      // Set both buttons to strings with the cancel button being default
      Ci.nsIPromptService.BUTTON_POS_1_DEFAULT |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0 |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_1,
      bundle.GetStringFromName("webapps.install.install"),
      bundle.GetStringFromName("webapps.install.dontinstall"),
      null,
      null,
      {});

    // Perform the install if the user allows it
    if (choice == 0) {
      let nativeApp = new NativeApp(data.app, jsonManifest,
                                    data.app.categories,
                                    WebappRT.config.registryDir);
      let localDir;
      try {
        localDir = nativeApp.createProfile();
      } catch (ex) {
        DOMApplicationRegistry.denyInstall(data);
        return;
      }

      DOMApplicationRegistry.confirmInstall(data, localDir,
        Task.async(function*(aApp, aManifest, aZipPath) {
          yield nativeApp.install(aApp, aManifest, aZipPath);
        })
      );
    } else {
      DOMApplicationRegistry.denyInstall(data);
    }
  },

  doUninstall: function(aData, aWindow) {
    let jsonManifest = aData.isPackage ? aData.app.updateManifest : aData.app.manifest;
    let manifest = new ManifestHelper(jsonManifest, aData.app.origin,
                                      aData.app.manifestURL);
    let name = manifest.name;
    let bundle = Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");

    let choice = Services.prompt.confirmEx(
      aWindow,
      bundle.formatStringFromName("webapps.uninstall.title", [name], 1),
      bundle.formatStringFromName("webapps.uninstall.description", [name], 1),
      // Set both buttons to strings with the cancel button being default
      Ci.nsIPromptService.BUTTON_POS_1_DEFAULT |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0 |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_1,
      bundle.GetStringFromName("webapps.uninstall.uninstall"),
      bundle.GetStringFromName("webapps.uninstall.dontuninstall"),
      null,
      null,
      {});

    // Perform the uninstall if the user allows it
    if (choice == 0) {
      DOMApplicationRegistry.confirmUninstall(aData).then((aApp) => {
        WebappOSUtils.uninstall(aApp).then(null, Cu.reportError);
      });
    } else {
      DOMApplicationRegistry.denyUninstall(aData);
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};

Services.obs.addObserver(WebappManager, "webapps-ask-install", false);
Services.obs.addObserver(WebappManager, "webapps-ask-uninstall", false);
Services.obs.addObserver(WebappManager, "webapps-launch", false);
Services.obs.addObserver(WebappManager, "webapps-uninstall", false);
Services.obs.addObserver(WebappManager, "webapps-update", false);

DOMApplicationRegistry.registerUpdateHandler(WebappManager.update);
