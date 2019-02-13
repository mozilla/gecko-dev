/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Based on:
// https://bugzilla.mozilla.org/show_bug.cgi?id=549539
// https://bug549539.bugzilla.mozilla.org/attachment.cgi?id=429661
// https://developer.mozilla.org/en/XPCOM/XPCOM_changes_in_Gecko_1.9.3
// http://mxr.mozilla.org/mozilla-central/source/toolkit/components/console/hudservice/HUDService.jsm#3240
// https://developer.mozilla.org/en/how_to_build_an_xpcom_component_in_javascript

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

if (typeof(Cc) == "undefined") {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
}

const CHILD_SCRIPT = "chrome://specialpowers/content/specialpowers.js"
const CHILD_SCRIPT_API = "chrome://specialpowers/content/specialpowersAPI.js"
const CHILD_LOGGER_SCRIPT = "chrome://specialpowers/content/MozillaLogger.js"


// Glue to add in the observer API to this object.  This allows us to share code with chrome tests
var loader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Components.interfaces.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserverAPI.js");

/* XPCOM gunk */
this.SpecialPowersObserver = function SpecialPowersObserver() {
  this._isFrameScriptLoaded = false;
  this._mmIsGlobal = true;
  this._messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                         getService(Ci.nsIMessageBroadcaster);
}


SpecialPowersObserver.prototype = new SpecialPowersObserverAPI();

  SpecialPowersObserver.prototype.classDescription = "Special powers Observer for use in testing.";
  SpecialPowersObserver.prototype.classID = Components.ID("{59a52458-13e0-4d93-9d85-a637344f29a1}");
  SpecialPowersObserver.prototype.contractID = "@mozilla.org/special-powers-observer;1";
  SpecialPowersObserver.prototype.QueryInterface = XPCOMUtils.generateQI([Components.interfaces.nsIObserver]);
  SpecialPowersObserver.prototype._xpcom_categories = [{category: "profile-after-change", service: true }];

  SpecialPowersObserver.prototype.observe = function(aSubject, aTopic, aData)
  {
    switch (aTopic) {
      case "profile-after-change":
        this.init();
        break;

      case "chrome-document-global-created":
        this._loadFrameScript();
        break;

      case "http-on-modify-request":
        if (aSubject instanceof Ci.nsIChannel) {
          let uri = aSubject.URI.spec;
          this._sendAsyncMessage("specialpowers-http-notify-request", { uri: uri });
        }
        break;

      case "xpcom-shutdown":
        this.uninit();
        break;

      default:
        this._observe(aSubject, aTopic, aData);
        break;
    }
  };

  SpecialPowersObserver.prototype._loadFrameScript = function()
  {
    if (!this._isFrameScriptLoaded) {
      // Register for any messages our API needs us to handle
      this._messageManager.addMessageListener("SPPrefService", this);
      this._messageManager.addMessageListener("SPProcessCrashService", this);
      this._messageManager.addMessageListener("SPPingService", this);
      this._messageManager.addMessageListener("SpecialPowers.Quit", this);
      this._messageManager.addMessageListener("SpecialPowers.Focus", this);
      this._messageManager.addMessageListener("SPPermissionManager", this);
      this._messageManager.addMessageListener("SPWebAppService", this);
      this._messageManager.addMessageListener("SPObserverService", this);
      this._messageManager.addMessageListener("SPLoadChromeScript", this);
      this._messageManager.addMessageListener("SPChromeScriptMessage", this);
      this._messageManager.addMessageListener("SPQuotaManager", this);
      this._messageManager.addMessageListener("SPSetTestPluginEnabledState", this);
      this._messageManager.addMessageListener("SPPeriodicServiceWorkerUpdates", this);

      this._messageManager.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
      this._messageManager.loadFrameScript(CHILD_SCRIPT_API, true);
      this._messageManager.loadFrameScript(CHILD_SCRIPT, true);
      this._isFrameScriptLoaded = true;
    }
  };

  SpecialPowersObserver.prototype._sendAsyncMessage = function(msgname, msg)
  {
    if (this._mmIsGlobal) {
      this._messageManager.broadcastAsyncMessage(msgname, msg);
    }
    else {
      this._messageManager.sendAsyncMessage(msgname, msg);
    }
  };

  SpecialPowersObserver.prototype._receiveMessage = function(aMessage) {
    return this._receiveMessageAPI(aMessage);
  };

  SpecialPowersObserver.prototype.init = function(messageManager)
  {
    var obs = Services.obs;
    obs.addObserver(this, "xpcom-shutdown", false);
    obs.addObserver(this, "chrome-document-global-created", false);

    // Register special testing modules.
    var testsURI = Cc["@mozilla.org/file/directory_service;1"].
                     getService(Ci.nsIProperties).
                     get("ProfD", Ci.nsILocalFile);
    testsURI.append("tests.manifest");
    var ioSvc = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);
    var manifestFile = ioSvc.newFileURI(testsURI).
                         QueryInterface(Ci.nsIFileURL).file;

    Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
                   autoRegister(manifestFile);

    obs.addObserver(this, "http-on-modify-request", false);

    if (messageManager) {
      this._messageManager = messageManager;
      this._mmIsGlobal = false;

      this._loadFrameScript();
    }
  };

  SpecialPowersObserver.prototype.uninit = function()
  {
    var obs = Services.obs;
    obs.removeObserver(this, "chrome-document-global-created");
    obs.removeObserver(this, "http-on-modify-request");
    obs.removeObserver(this, "xpcom-shutdown");
    this._registerObservers._topics.forEach(function(element) {
      obs.removeObserver(this._registerObservers, element);
    });
    this._removeProcessCrashObservers();

    if (this._isFrameScriptLoaded) {
      this._messageManager.removeMessageListener("SPPrefService", this);
      this._messageManager.removeMessageListener("SPProcessCrashService", this);
      this._messageManager.removeMessageListener("SPPingService", this);
      this._messageManager.removeMessageListener("SpecialPowers.Quit", this);
      this._messageManager.removeMessageListener("SpecialPowers.Focus", this);
      this._messageManager.removeMessageListener("SPPermissionManager", this);
      this._messageManager.removeMessageListener("SPWebAppService", this);
      this._messageManager.removeMessageListener("SPObserverService", this);
      this._messageManager.removeMessageListener("SPLoadChromeScript", this);
      this._messageManager.removeMessageListener("SPChromeScriptMessage", this);
      this._messageManager.removeMessageListener("SPQuotaManager", this);
      this._messageManager.removeMessageListener("SPSetTestPluginEnabledState", this);
      this._messageManager.removeMessageListener("SPPeriodicServiceWorkerUpdates", this);

      this._messageManager.removeDelayedFrameScript(CHILD_LOGGER_SCRIPT);
      this._messageManager.removeDelayedFrameScript(CHILD_SCRIPT_API);
      this._messageManager.removeDelayedFrameScript(CHILD_SCRIPT);
      this._isFrameScriptLoaded = false;
    }

    this._mmIsGlobal = true;
    this._messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
      getService(Ci.nsIMessageBroadcaster);
  };

  SpecialPowersObserver.prototype._addProcessCrashObservers = function() {
    if (this._processCrashObserversRegistered) {
      return;
    }

    var obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);

    obs.addObserver(this, "plugin-crashed", false);
    obs.addObserver(this, "ipc:content-shutdown", false);
    this._processCrashObserversRegistered = true;
  };

  SpecialPowersObserver.prototype._removeProcessCrashObservers = function() {
    if (!this._processCrashObserversRegistered) {
      return;
    }

    var obs = Components.classes["@mozilla.org/observer-service;1"]
                        .getService(Components.interfaces.nsIObserverService);

    obs.removeObserver(this, "plugin-crashed");
    obs.removeObserver(this, "ipc:content-shutdown");
    this._processCrashObserversRegistered = false;
  };

  SpecialPowersObserver.prototype._registerObservers = {
    _self: null,
    _topics: [],
    _add: function(topic) {
      if (this._topics.indexOf(topic) < 0) {
        this._topics.push(topic);
        Services.obs.addObserver(this, topic, false);
      }
    },
    observe: function (aSubject, aTopic, aData) {
      var msg = { aData: aData };
      switch (aTopic) {
        case "perm-changed":
          var permission = aSubject.QueryInterface(Ci.nsIPermission);
          msg.permission = { appId: permission.appId, type: permission.type };
        default:
          this._self._sendAsyncMessage("specialpowers-" + aTopic, msg);
      }
    }
  };

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  SpecialPowersObserver.prototype.receiveMessage = function(aMessage) {
    switch(aMessage.name) {
      case "SPPingService":
        if (aMessage.json.op == "ping") {
          aMessage.target
                  .QueryInterface(Ci.nsIFrameLoaderOwner)
                  .frameLoader
                  .messageManager
                  .sendAsyncMessage("SPPingService", { op: "pong" });
        }
        break;
      case "SpecialPowers.Quit":
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eForceQuit);
        break;
      case "SpecialPowers.Focus":
        aMessage.target.focus();
        break;
      default:
        return this._receiveMessage(aMessage);
    }
  };

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SpecialPowersObserver]);
