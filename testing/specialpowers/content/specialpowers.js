/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

function SpecialPowers(window) {
  this.window = Components.utils.getWeakReference(window);
  this._windowID = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindowUtils)
                         .currentInnerWindowID;
  this._encounteredCrashDumpFiles = [];
  this._unexpectedCrashDumpFiles = { };
  this._crashDumpDir = null;
  this.DOMWindowUtils = bindDOMWindowUtils(window);
  Object.defineProperty(this, 'Components', {
      configurable: true, enumerable: true, get: function() {
          var win = this.window.get();
          if (!win)
              return null;
          return getRawComponents(win);
      }});
  this._pongHandlers = [];
  this._messageListener = this._messageReceived.bind(this);
  this._grandChildFrameMM = null;
  this.SP_SYNC_MESSAGES = ["SPChromeScriptMessage",
                           "SPLoadChromeScript",
                           "SPObserverService",
                           "SPPermissionManager",
                           "SPPrefService",
                           "SPProcessCrashService",
                           "SPSetTestPluginEnabledState",
                           "SPWebAppService",
                           "SPPeriodicServiceWorkerUpdates"];

  this.SP_ASYNC_MESSAGES = ["SpecialPowers.Focus",
                            "SpecialPowers.Quit",
                            "SPPingService",
                            "SPQuotaManager"];
  addMessageListener("SPPingService", this._messageListener);
  let self = this;
  Services.obs.addObserver(function onInnerWindowDestroyed(subject, topic, data) {
    var id = subject.QueryInterface(Components.interfaces.nsISupportsPRUint64).data;
    if (self._windowID === id) {
      Services.obs.removeObserver(onInnerWindowDestroyed, "inner-window-destroyed");
      try {
        removeMessageListener("SPPingService", self._messageListener);
      } catch (e if e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
        // Ignore the exception which the message manager has been destroyed.
        ;
      }
    }
  }, "inner-window-destroyed", false);
}

SpecialPowers.prototype = new SpecialPowersAPI();

SpecialPowers.prototype.toString = function() { return "[SpecialPowers]"; };
SpecialPowers.prototype.sanityCheck = function() { return "foo"; };

// This gets filled in in the constructor.
SpecialPowers.prototype.DOMWindowUtils = undefined;
SpecialPowers.prototype.Components = undefined;
SpecialPowers.prototype.IsInNestedFrame = false;

SpecialPowers.prototype._sendSyncMessage = function(msgname, msg) {
  if (this.SP_SYNC_MESSAGES.indexOf(msgname) == -1) {
    dump("TEST-INFO | specialpowers.js |  Unexpected SP message: " + msgname + "\n");
  }
  return sendSyncMessage(msgname, msg);
};

SpecialPowers.prototype._sendAsyncMessage = function(msgname, msg) {
  if (this.SP_ASYNC_MESSAGES.indexOf(msgname) == -1) {
    dump("TEST-INFO | specialpowers.js |  Unexpected SP message: " + msgname + "\n");
  }
  sendAsyncMessage(msgname, msg);
};

SpecialPowers.prototype._addMessageListener = function(msgname, listener) {
  addMessageListener(msgname, listener);
  sendAsyncMessage("SPPAddNestedMessageListener", { name: msgname });
};

SpecialPowers.prototype._removeMessageListener = function(msgname, listener) {
  removeMessageListener(msgname, listener);
};

SpecialPowers.prototype.registerProcessCrashObservers = function() {
  addMessageListener("SPProcessCrashService", this._messageListener);
  sendSyncMessage("SPProcessCrashService", { op: "register-observer" });
};

SpecialPowers.prototype.unregisterProcessCrashObservers = function() {
  removeMessageListener("SPProcessCrashService", this._messageListener);
  sendSyncMessage("SPProcessCrashService", { op: "unregister-observer" });
};

SpecialPowers.prototype._messageReceived = function(aMessage) {
  switch (aMessage.name) {
    case "SPProcessCrashService":
      if (aMessage.json.type == "crash-observed") {
        for (let e of aMessage.json.dumpIDs) {
          this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
        }
      }
      break;

    case "SPPingService":
      if (aMessage.json.op == "pong") {
        var handler = this._pongHandlers.shift();
        if (handler) {
          handler();
        }
        if (this._grandChildFrameMM) {
          this._grandChildFrameMM.sendAsyncMessage("SPPingService", { op: "pong" });
        }
      }
      break;
  }
  return true;
};

SpecialPowers.prototype.quit = function() {
  sendAsyncMessage("SpecialPowers.Quit", {});
};

SpecialPowers.prototype.executeAfterFlushingMessageQueue = function(aCallback) {
  this._pongHandlers.push(aCallback);
  sendAsyncMessage("SPPingService", { op: "ping" });
};

SpecialPowers.prototype.nestedFrameSetup = function() {
  let self = this;
  Services.obs.addObserver(function onRemoteBrowserShown(subject, topic, data) {
    let frameLoader = subject;
    // get a ref to the app <iframe>
    frameLoader.QueryInterface(Components.interfaces.nsIFrameLoader);
    let frame = frameLoader.ownerElement;
    let frameId = frame.getAttribute('id');
    if (frameId === "nested-parent-frame") {
      Services.obs.removeObserver(onRemoteBrowserShown, "remote-browser-shown");

      let mm = frame.QueryInterface(Components.interfaces.nsIFrameLoaderOwner).frameLoader.messageManager;
      self._grandChildFrameMM = mm;

      self.SP_SYNC_MESSAGES.forEach(function (msgname) {
        mm.addMessageListener(msgname, function (msg) {
          return self._sendSyncMessage(msgname, msg.data)[0];
        });
      });
      self.SP_ASYNC_MESSAGES.forEach(function (msgname) {
        mm.addMessageListener(msgname, function (msg) {
          self._sendAsyncMessage(msgname, msg.data);
        });
      });
      mm.addMessageListener("SPPAddNestedMessageListener", function(msg) {
        self._addMessageListener(msg.json.name, function(aMsg) {
          mm.sendAsyncMessage(aMsg.name, aMsg.data);
          });
      });

      let specialPowersBase = "chrome://specialpowers/content/";
      mm.loadFrameScript(specialPowersBase + "MozillaLogger.js", false);
      mm.loadFrameScript(specialPowersBase + "specialpowersAPI.js", false);
      mm.loadFrameScript(specialPowersBase + "specialpowers.js", false);

      let frameScript = "SpecialPowers.prototype.IsInNestedFrame=true;";
      mm.loadFrameScript("data:," + frameScript, false);
    }
  }, "remote-browser-shown", false);
};

// Attach our API to the window.
function attachSpecialPowersToWindow(aWindow) {
  try {
    if ((aWindow !== null) &&
        (aWindow !== undefined) &&
        (aWindow.wrappedJSObject) &&
        !(aWindow.wrappedJSObject.SpecialPowers)) {
      let sp = new SpecialPowers(aWindow);
      aWindow.wrappedJSObject.SpecialPowers = sp;
      if (sp.IsInNestedFrame) {
        sp.addPermission("allowXULXBL", true, aWindow.document);
      }
    }
  } catch(ex) {
    dump("TEST-INFO | specialpowers.js |  Failed to attach specialpowers to window exception: " + ex + "\n");
  }
}

// This is a frame script, so it may be running in a content process.
// In any event, it is targeted at a specific "tab", so we listen for
// the DOMWindowCreated event to be notified about content windows
// being created in this context.

function SpecialPowersManager() {
  addEventListener("DOMWindowCreated", this, false);
}

SpecialPowersManager.prototype = {
  handleEvent: function handleEvent(aEvent) {
    var window = aEvent.target.defaultView;
    attachSpecialPowersToWindow(window);
  }
};


var specialpowersmanager = new SpecialPowersManager();

this.SpecialPowers = SpecialPowers;
this.attachSpecialPowersToWindow = attachSpecialPowersToWindow;

// In the case of Chrome mochitests that inject specialpowers.js as
// a regular content script
if (typeof window != 'undefined') {
  window.addMessageListener = function() {}
  window.removeMessageListener = function() {}
  window.wrappedJSObject.SpecialPowers = new SpecialPowers(window);
}
