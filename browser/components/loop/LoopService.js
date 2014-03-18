"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/MozSocialAPI.jsm");
Cu.import("resource://gre/modules/SocialService.jsm");

//const loopServerUri = "http://loop.dev.mozaws.net";
const loopServerUri = "http://localhost:5000";
const pushServerUri = "wss://push.services.mozilla.com";
const channelID = "8b1081ce-9b35-42b5-b8f5-3ff8cb813a50";

function LoopService() {}

LoopService.prototype = {
  classID: Components.ID("{324562fa-325e-449c-a433-2b1e6a3fb145}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(LoopService),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsITimerCallback]),

  observe: function LS_observe(aSubject, aTopic, aData) {
    if (aTopic != "profile-after-change") {
      Cu.reportError("Unexpected observer notification.");
      return;
    }

    this.startupTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this.startupTimer.initWithCallback(this, 500, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  notify: function LS_notify(aTimer) {
    delete this.startupTimer;
    Cu.reportError("Yay!");

    // Get push url
    this.websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
                       .createInstance(Ci.nsIWebSocketChannel);

    this.websocket.protocol ="push-notification";
    this.websocket.asyncOpen(Services.io.newURI(pushServerUri, null, null), pushServerUri, this, null);
  },

  onStart: function() {
    var helloMsg = { messageType: "hello", uaid: "", channelIDs: []};
    this.websocket.sendMsg(JSON.stringify(helloMsg));
  },

  onStop: function() {
    Cu.reportError("Web socket closed!");
  },

  onServerClose: function() {
    Cu.reportError("Web socket closed (server)!");
  },

  onMessageAvailable: function(e, message) {
    var msg = JSON.parse(message);

    switch(msg.messageType) {
      case "hello":
        this.websocket.sendMsg(JSON.stringify({messageType: "register", channelID: channelID}));
        break;
      case "register":
        dump("\n\nPush url is: " + msg.pushEndpoint + "\n");
        Cu.reportError("Push url is: " + msg.pushEndpoint);
try {

        this.registerXhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                             .createInstance(Ci.nsIXMLHttpRequest);
        // XXX Sync!
        this.registerXhr.open('POST', loopServerUri + "/registration", false);
        this.registerXhr.setRequestHeader('Content-Type', 'application/json');
        this.registerXhr.channel.loadFlags = Ci.nsIChannel.INHIBIT_CACHING | Ci.nsIChannel.LOAD_BYPASS_CACHE | Ci.nsIChannel.LOAD_EXPLICIT_CREDENTIALS;
        this.registerXhr.sendAsBinary(JSON.stringify({simple_push_url: msg.pushEndpoint}));
        this.callXhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                             .createInstance(Ci.nsIXMLHttpRequest);
        // XXX Sync!
//        this.callXhr.open('POST', loopServerUri + "/call-url/", false);
//        this.callXhr.setRequestHeader('Content-Type', 'application/json');
//        this.callXhr.sendAsBinary(JSON.stringify({remote_id: "fake", valid_duration: 86400}));
} catch (x) {
  Cu.reportError(x);
}
        break;
      case "notification":
        if (channelID === channelID) {
          Cu.reportError("Notification!");
          SocialService.getProvider("chrome://browser/content/loop/", this.openChat.bind(this));
        }
        break;
    }
  },

  openChat: function(provider) {
    let mostRecent = Services.wm.getMostRecentWindow("navigator:browser");
    openChatWindow(mostRecent, provider, "chrome://browser/content/loop/chat.html");
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([LoopService]);
