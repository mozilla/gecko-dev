/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PeerConnectionIdp",
  "resource://gre/modules/media/PeerConnectionIdp.jsm");

const PC_CONTRACT = "@mozilla.org/dom/peerconnection;1";
const PC_OBS_CONTRACT = "@mozilla.org/dom/peerconnectionobserver;1";
const PC_ICE_CONTRACT = "@mozilla.org/dom/rtcicecandidate;1";
const PC_SESSION_CONTRACT = "@mozilla.org/dom/rtcsessiondescription;1";
const PC_MANAGER_CONTRACT = "@mozilla.org/dom/peerconnectionmanager;1";
const PC_STATS_CONTRACT = "@mozilla.org/dom/rtcstatsreport;1";
const PC_IDENTITY_CONTRACT = "@mozilla.org/dom/rtcidentityassertion;1";
const PC_STATIC_CONTRACT = "@mozilla.org/dom/peerconnectionstatic;1";

const PC_CID = Components.ID("{00e0e20d-1494-4776-8e0e-0f0acbea3c79}");
const PC_OBS_CID = Components.ID("{d1748d4c-7f6a-4dc5-add6-d55b7678537e}");
const PC_ICE_CID = Components.ID("{02b9970c-433d-4cc2-923d-f7028ac66073}");
const PC_SESSION_CID = Components.ID("{1775081b-b62d-4954-8ffe-a067bbf508a7}");
const PC_MANAGER_CID = Components.ID("{7293e901-2be3-4c02-b4bd-cbef6fc24f78}");
const PC_STATS_CID = Components.ID("{7fe6e18b-0da3-4056-bf3b-440ef3809e06}");
const PC_IDENTITY_CID = Components.ID("{1abc7499-3c54-43e0-bd60-686e2703f072}");
const PC_STATIC_CID = Components.ID("{0fb47c47-a205-4583-a9fc-cbadf8c95880}");

// Global list of PeerConnection objects, so they can be cleaned up when
// a page is torn down. (Maps inner window ID to an array of PC objects).
function GlobalPCList() {
  this._list = {};
  this._networkdown = false; // XXX Need to query current state somehow
  this._lifecycleobservers = {};
  Services.obs.addObserver(this, "inner-window-destroyed", true);
  Services.obs.addObserver(this, "profile-change-net-teardown", true);
  Services.obs.addObserver(this, "network:offline-about-to-go-offline", true);
  Services.obs.addObserver(this, "network:offline-status-changed", true);
}
GlobalPCList.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.IPeerConnectionManager]),
  classID: PC_MANAGER_CID,
  _xpcom_factory: {
    createInstance: function(outer, iid) {
      if (outer) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }
      return _globalPCList.QueryInterface(iid);
    }
  },

  notifyLifecycleObservers: function(pc, type) {
    for (var key of Object.keys(this._lifecycleobservers)) {
      this._lifecycleobservers[key](pc, pc._winID, type);
    }
  },

  addPC: function(pc) {
    let winID = pc._winID;
    if (this._list[winID]) {
      this._list[winID].push(Cu.getWeakReference(pc));
    } else {
      this._list[winID] = [Cu.getWeakReference(pc)];
    }
    this.removeNullRefs(winID);
  },

  removeNullRefs: function(winID) {
    if (this._list[winID] === undefined) {
      return;
    }
    this._list[winID] = this._list[winID].filter(
      function (e,i,a) { return e.get() !== null; });

    if (this._list[winID].length === 0) {
      delete this._list[winID];
    }
  },

  hasActivePeerConnection: function(winID) {
    this.removeNullRefs(winID);
    return this._list[winID] ? true : false;
  },

  observe: function(subject, topic, data) {
    let cleanupPcRef = function(pcref) {
      let pc = pcref.get();
      if (pc) {
        pc._pc.close();
        delete pc._observer;
        pc._pc = null;
      }
    };

    let cleanupWinId = function(list, winID) {
      if (list.hasOwnProperty(winID)) {
        list[winID].forEach(cleanupPcRef);
        delete list[winID];
      }
    };

    if (topic == "inner-window-destroyed") {
      let winID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      cleanupWinId(this._list, winID);

      if (this._lifecycleobservers.hasOwnProperty(winID)) {
        delete this._lifecycleobservers[winID];
      }
    } else if (topic == "profile-change-net-teardown" ||
               topic == "network:offline-about-to-go-offline") {
      // Delete all peerconnections on shutdown - mostly synchronously (we
      // need them to be done deleting transports and streams before we
      // return)! All socket operations must be queued to STS thread
      // before we return to here.
      // Also kill them if "Work Offline" is selected - more can be created
      // while offline, but attempts to connect them should fail.
      for (let winId in this._list) {
        cleanupWinId(this._list, winId);
      }
      this._networkdown = true;
    }
    else if (topic == "network:offline-status-changed") {
      if (data == "offline") {
        // this._list shold be empty here
        this._networkdown = true;
      } else if (data == "online") {
        this._networkdown = false;
      }
    }
  },

  _registerPeerConnectionLifecycleCallback: function(winID, cb) {
    this._lifecycleobservers[winID] = cb;
  },
};
let _globalPCList = new GlobalPCList();

function RTCIceCandidate() {
  this.candidate = this.sdpMid = this.sdpMLineIndex = null;
}
RTCIceCandidate.prototype = {
  classDescription: "mozRTCIceCandidate",
  classID: PC_ICE_CID,
  contractID: PC_ICE_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  init: function(win) { this._win = win; },

  __init: function(dict) {
    this.candidate = dict.candidate;
    this.sdpMid = dict.sdpMid;
    this.sdpMLineIndex = ("sdpMLineIndex" in dict)? dict.sdpMLineIndex : null;
  }
};

function RTCSessionDescription() {
  this.type = this.sdp = null;
}
RTCSessionDescription.prototype = {
  classDescription: "mozRTCSessionDescription",
  classID: PC_SESSION_CID,
  contractID: PC_SESSION_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  init: function(win) { this._win = win; },

  __init: function(dict) {
    this.type = dict.type;
    this.sdp  = dict.sdp;
  }
};

function RTCStatsReport(win, dict) {
  function appendStats(stats, report) {
    stats.forEach(function(stat) {
        report[stat.id] = stat;
      });
  }

  this._win = win;
  this._pcid = dict.pcid;
  this._report = {};
  appendStats(dict.inboundRTPStreamStats, this._report);
  appendStats(dict.outboundRTPStreamStats, this._report);
  appendStats(dict.mediaStreamTrackStats, this._report);
  appendStats(dict.mediaStreamStats, this._report);
  appendStats(dict.transportStats, this._report);
  appendStats(dict.iceComponentStats, this._report);
  appendStats(dict.iceCandidatePairStats, this._report);
  appendStats(dict.iceCandidateStats, this._report);
  appendStats(dict.codecStats, this._report);
}
RTCStatsReport.prototype = {
  classDescription: "RTCStatsReport",
  classID: PC_STATS_CID,
  contractID: PC_STATS_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  // TODO: Change to use webidl getters once available (Bug 952122)
  //
  // Since webidl getters are not available, we make the stats available as
  // enumerable read-only properties directly on our content-facing object.
  // Must be called after our webidl sandwich is made.

  makeStatsPublic: function() {
    let props = {};
    this.forEach(function(stat) {
        props[stat.id] = { enumerable: true, configurable: false,
                           writable: false, value: stat };
      });
    Object.defineProperties(this.__DOM_IMPL__.wrappedJSObject, props);
  },

  forEach: function(cb, thisArg) {
    for (var key in this._report) {
      cb.call(thisArg || this._report, this.get(key), key, this._report);
    }
  },

  get: function(key) {
    function publifyReadonly(win, obj) {
      let props = {};
      for (let k in obj) {
        props[k] = {enumerable:true, configurable:false, writable:false, value:obj[k]};
      }
      let pubobj = Cu.createObjectIn(win);
      Object.defineProperties(pubobj, props);
      return pubobj;
    }

    // Return a content object rather than a wrapped chrome one.
    return publifyReadonly(this._win, this._report[key]);
  },

  has: function(key) {
    return this._report[key] !== undefined;
  },

  get mozPcid() { return this._pcid; }
};

function RTCIdentityAssertion() {}
RTCIdentityAssertion.prototype = {
  classDescription: "RTCIdentityAssertion",
  classID: PC_IDENTITY_CID,
  contractID: PC_IDENTITY_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  init: function(win) { this._win = win; },

  __init: function(idp, name) {
    this.idp = idp;
    this.name  = name;
  }
};

function RTCPeerConnection() {
  this._queue = [];

  this._pc = null;
  this._observer = null;
  this._closed = false;

  this._onCreateOfferSuccess = null;
  this._onCreateOfferFailure = null;
  this._onCreateAnswerSuccess = null;
  this._onCreateAnswerFailure = null;
  this._onGetStatsSuccess = null;
  this._onGetStatsFailure = null;

  this._pendingType = null;
  this._localType = null;
  this._remoteType = null;
  this._trickleIce = false;
  this._peerIdentity = null;

  /**
   * Everytime we get a request from content, we put it in the queue. If there
   * are no pending operations though, we will execute it immediately. In
   * PeerConnectionObserver, whenever we are notified that an operation has
   * finished, we will check the queue for the next operation and execute if
   * neccesary. The _pending flag indicates whether an operation is currently in
   * progress.
   */
  this._pending = false;

  // States
  this._iceGatheringState = this._iceConnectionState = "new";
}
RTCPeerConnection.prototype = {
  classDescription: "mozRTCPeerConnection",
  classID: PC_CID,
  contractID: PC_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
  init: function(win) { this._win = win; },

  __init: function(rtcConfig) {
    this._trickleIce = Services.prefs.getBoolPref("media.peerconnection.trickle_ice");
    if (!rtcConfig.iceServers ||
        !Services.prefs.getBoolPref("media.peerconnection.use_document_iceservers")) {
      rtcConfig.iceServers =
        JSON.parse(Services.prefs.getCharPref("media.peerconnection.default_iceservers"));
    }
    this._mustValidateRTCConfiguration(rtcConfig,
        "RTCPeerConnection constructor passed invalid RTCConfiguration");
    if (_globalPCList._networkdown) {
      throw new this._win.DOMError("",
          "Can't create RTCPeerConnections when the network is down");
    }

    this.makeGetterSetterEH("onaddstream");
    this.makeGetterSetterEH("onicecandidate");
    this.makeGetterSetterEH("onnegotiationneeded");
    this.makeGetterSetterEH("onsignalingstatechange");
    this.makeGetterSetterEH("onremovestream");
    this.makeGetterSetterEH("ondatachannel");
    this.makeGetterSetterEH("oniceconnectionstatechange");
    this.makeGetterSetterEH("onidentityresult");
    this.makeGetterSetterEH("onpeeridentity");
    this.makeGetterSetterEH("onidpassertionerror");
    this.makeGetterSetterEH("onidpvalidationerror");

    this._pc = new this._win.PeerConnectionImpl();

    this.__DOM_IMPL__._innerObject = this;
    this._observer = new this._win.PeerConnectionObserver(this.__DOM_IMPL__);

    // Add a reference to the PeerConnection to global list (before init).
    this._winID = this._win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    _globalPCList.addPC(this);

    this._queueOrRun({
      func: this._initialize,
      args: [rtcConfig],
      // If not trickling, suppress start.
      wait: !this._trickleIce
    });
  },

  _initialize: function(rtcConfig) {
    this._impl.initialize(this._observer, this._win, rtcConfig,
                          Services.tm.currentThread);
    this._initIdp();
    _globalPCList.notifyLifecycleObservers(this, "initialized");
  },

  get _impl() {
    if (!this._pc) {
      throw new this._win.DOMError("",
          "RTCPeerConnection is gone (did you enter Offline mode?)");
    }
    return this._pc;
  },

  callCB: function(callback, arg) {
    if (callback) {
      try {
        callback(arg);
      } catch(e) {
        // A content script (user-provided) callback threw an error. We don't
        // want this to take down peerconnection, but we still want the user
        // to see it, so we catch it, report it, and move on.
        this.logErrorAndCallOnError(e.message, e.fileName, e.lineNumber);
      }
    }
  },

  _initIdp: function() {
    let prefName = "media.peerconnection.identity.timeout";
    let idpTimeout = Services.prefs.getIntPref(prefName);
    let warningFunc = this.logWarning.bind(this);
    this._localIdp = new PeerConnectionIdp(this._win, idpTimeout, warningFunc,
                                           this.dispatchEvent.bind(this));
    this._remoteIdp = new PeerConnectionIdp(this._win, idpTimeout, warningFunc,
                                            this.dispatchEvent.bind(this));
  },

  /**
   * Add a function to the queue or run it immediately if the queue is empty.
   * Argument is an object with the func, args and wait properties; wait should
   * be set to true if the function has a success/error callback that will call
   * _executeNext, false if it doesn't have a callback.
   */
  _queueOrRun: function(obj) {
    this._checkClosed();
    if (!this._pending) {
      if (obj.type !== undefined) {
        this._pendingType = obj.type;
      }
      obj.func.apply(this, obj.args);
      if (obj.wait) {
        this._pending = true;
      }
    } else {
      this._queue.push(obj);
    }
  },

  // Pick the next item from the queue and run it.
  _executeNext: function() {
    if (this._queue.length) {
      let obj = this._queue.shift();
      if (obj.type !== undefined) {
        this._pendingType = obj.type;
      }
      obj.func.apply(this, obj.args);
      if (!obj.wait) {
        this._executeNext();
      }
    } else {
      this._pending = false;
    }
  },

  /**
   * An RTCConfiguration looks like this:
   *
   * { "iceServers": [ { url:"stun:stun.example.org" },
   *                   { url:"turn:turn.example.org",
   *                     username:"jib", credential:"mypass"} ] }
   *
   * WebIDL normalizes structure for us, so we test well-formed stun/turn urls,
   * but not validity of servers themselves, before passing along to C++.
   * ErrorMsg is passed in to detail which array-entry failed, if any.
   */
  _mustValidateRTCConfiguration: function(rtcConfig, errorMsg) {
    var errorCtor = this._win.DOMError;
    function nicerNewURI(uriStr, errorMsg) {
      let ios = Cc['@mozilla.org/network/io-service;1'].getService(Ci.nsIIOService);
      try {
        return ios.newURI(uriStr, null, null);
      } catch (e if (e.result == Cr.NS_ERROR_MALFORMED_URI)) {
        throw new errorCtor("", errorMsg + " - malformed URI: " + uriStr);
      }
    }
    function mustValidateServer(server) {
      if (!server.url) {
        throw new errorCtor("", errorMsg + " - missing url");
      }
      let url = nicerNewURI(server.url, errorMsg);
      if (url.scheme in { turn:1, turns:1 }) {
        if (!server.username) {
          throw new errorCtor("", errorMsg + " - missing username: " + server.url);
        }
        if (!server.credential) {
          throw new errorCtor("", errorMsg + " - missing credential: " +
                              server.url);
        }
      }
      else if (!(url.scheme in { stun:1, stuns:1 })) {
        throw new errorCtor("", errorMsg + " - improper scheme: " + url.scheme);
      }
    }
    if (rtcConfig.iceServers) {
      let len = rtcConfig.iceServers.length;
      for (let i=0; i < len; i++) {
        mustValidateServer (rtcConfig.iceServers[i], errorMsg);
      }
    }
  },

  /**
   * MediaConstraints look like this:
   *
   * {
   *   mandatory: {"OfferToReceiveAudio": true, "OfferToReceiveVideo": true },
   *   optional: [{"VoiceActivityDetection": true}, {"FooBar": 10}]
   * }
   *
   * WebIDL normalizes the top structure for us, but the mandatory constraints
   * member comes in as a raw object so we can detect unknown constraints. We
   * compare its members against ones we support, and fail if not found.
   */
  _mustValidateConstraints: function(constraints, errorMsg) {
    if (constraints.mandatory) {
      let supported;
      try {
        // Passing the raw constraints.mandatory here validates its structure
        supported = this._observer.getSupportedConstraints(constraints.mandatory);
      } catch (e) {
        throw new this._win.DOMError("", errorMsg + " - " + e.message);
      }

      for (let constraint of Object.keys(constraints.mandatory)) {
        if (!(constraint in supported)) {
          throw new this._win.DOMError("",
              errorMsg + " - unknown mandatory constraint: " + constraint);
        }
      }
    }
    if (constraints.optional) {
      let len = constraints.optional.length;
      for (let i = 0; i < len; i++) {
        let constraints_per_entry = 0;
        for (let constraint in Object.keys(constraints.optional[i])) {
          if (constraints_per_entry) {
            throw new this._win.DOMError("", errorMsg +
                " - optional constraint must be single key/value pair");
          }
          constraints_per_entry += 1;
        }
      }
    }
  },

  // Ideally, this should be of the form _checkState(state),
  // where the state is taken from an enumeration containing
  // the valid peer connection states defined in the WebRTC
  // spec. See Bug 831756.
  _checkClosed: function() {
    if (this._closed) {
      throw new this._win.DOMError("", "Peer connection is closed");
    }
  },

  dispatchEvent: function(event) {
    this.__DOM_IMPL__.dispatchEvent(event);
  },

  // Log error message to web console and window.onerror, if present.
  logErrorAndCallOnError: function(msg, file, line) {
    this.logMsg(msg, file, line, Ci.nsIScriptError.exceptionFlag);

    // Safely call onerror directly if present (necessary for testing)
    try {
      if (typeof this._win.onerror === "function") {
        this._win.onerror(msg, file, line);
      }
    } catch(e) {
      // If onerror itself throws, service it.
      try {
        this.logError(e.message, e.fileName, e.lineNumber);
      } catch(e) {}
    }
  },

  logError: function(msg, file, line) {
    this.logMsg(msg, file, line, Ci.nsIScriptError.errorFlag);
  },

  logWarning: function(msg, file, line) {
    this.logMsg(msg, file, line, Ci.nsIScriptError.warningFlag);
  },

  logMsg: function(msg, file, line, flag) {
    let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
    let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
    scriptError.initWithWindowID(msg, file, null, line, 0, flag,
                                 "content javascript", this._winID);
    let console = Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService);
    console.logMessage(scriptError);
  },

  getEH: function(type) {
    return this.__DOM_IMPL__.getEventHandler(type);
  },

  setEH: function(type, handler) {
    this.__DOM_IMPL__.setEventHandler(type, handler);
  },

  makeGetterSetterEH: function(name) {
    Object.defineProperty(this, name,
                          {
                            get:function()  { return this.getEH(name); },
                            set:function(h) { return this.setEH(name, h); }
                          });
  },

  createOffer: function(onSuccess, onError, constraints) {
    if (!constraints) {
      constraints = {};
    }
    this._mustValidateConstraints(constraints, "createOffer passed invalid constraints");

    this._queueOrRun({
      func: this._createOffer,
      args: [onSuccess, onError, constraints],
      wait: true
    });
  },

  _createOffer: function(onSuccess, onError, constraints) {
    this._onCreateOfferSuccess = onSuccess;
    this._onCreateOfferFailure = onError;
    this._impl.createOffer(constraints);
  },

  _createAnswer: function(onSuccess, onError, constraints, provisional) {
    this._onCreateAnswerSuccess = onSuccess;
    this._onCreateAnswerFailure = onError;

    if (!this.remoteDescription) {

      this._observer.onCreateAnswerError(Ci.IPeerConnection.kInvalidState,
                                         "setRemoteDescription not called");
      return;
    }

    if (this.remoteDescription.type != "offer") {

      this._observer.onCreateAnswerError(Ci.IPeerConnection.kInvalidState,
                                         "No outstanding offer");
      return;
    }

    // TODO: Implement provisional answer.

    this._impl.createAnswer(constraints);
  },

  createAnswer: function(onSuccess, onError, constraints, provisional) {
    if (!constraints) {
      constraints = {};
    }

    this._mustValidateConstraints(constraints, "createAnswer passed invalid constraints");

    if (!provisional) {
      provisional = false;
    }

    this._queueOrRun({
      func: this._createAnswer,
      args: [onSuccess, onError, constraints, provisional],
      wait: true
    });
  },

  setLocalDescription: function(desc, onSuccess, onError) {
    let type;
    switch (desc.type) {
      case "offer":
        type = Ci.IPeerConnection.kActionOffer;
        break;
      case "answer":
        type = Ci.IPeerConnection.kActionAnswer;
        break;
      case "pranswer":
        throw new this._win.DOMError("", "pranswer not yet implemented");
      default:
        throw new this._win.DOMError("",
            "Invalid type " + desc.type + " provided to setLocalDescription");
    }

    this._queueOrRun({
      func: this._setLocalDescription,
      args: [type, desc.sdp, onSuccess, onError],
      wait: true,
      type: desc.type
    });
  },

  _setLocalDescription: function(type, sdp, onSuccess, onError) {
    this._onSetLocalDescriptionSuccess = onSuccess;
    this._onSetLocalDescriptionFailure = onError;
    this._impl.setLocalDescription(type, sdp);
  },

  setRemoteDescription: function(desc, onSuccess, onError) {
    let type;
    switch (desc.type) {
      case "offer":
        type = Ci.IPeerConnection.kActionOffer;
        break;
      case "answer":
        type = Ci.IPeerConnection.kActionAnswer;
        break;
      case "pranswer":
        throw new this._win.DOMError("", "pranswer not yet implemented");
      default:
        throw new this._win.DOMError("",
            "Invalid type " + desc.type + " provided to setRemoteDescription");
    }

    this._queueOrRun({
      func: this._setRemoteDescription,
      args: [type, desc.sdp, onSuccess, onError],
      wait: true,
      type: desc.type
    });
  },

  /**
   * Takes a result from the IdP and checks it against expectations.
   * If OK, generates events.
   * Returns true if it is either present and valid, or if there is no
   * need for identity.
   */
  _processIdpResult: function(message) {
    let good = !!message;
    // This might be a valid assertion, but if we are constrained to a single peer
    // identity, then we also need to make sure that the assertion matches
    if (good && this._impl.peerIdentity) {
      good = (message.identity === this._impl.peerIdentity);
    }
    if (good) {
      this._impl.peerIdentity = message.identity;
      this._peerIdentity = new this._win.RTCIdentityAssertion(
        this._remoteIdp.provider, message.identity);
      this.dispatchEvent(new this._win.Event("peeridentity"));
    }
    return good;
  },

  _setRemoteDescription: function(type, sdp, onSuccess, onError) {
    let idpComplete = false;
    let setRemoteComplete = false;
    let idpError = null;

    // we can run the IdP validation in parallel with setRemoteDescription this
    // complicates much more than would be ideal, but it ensures that the IdP
    // doesn't hold things up too much when it's not on the critical path
    let allDone = () => {
      if (!setRemoteComplete || !idpComplete || !onSuccess) {
        return;
      }
      this._remoteType = this._pendingType;
      this._pendingType = null;
      this.callCB(onSuccess);
      onSuccess = null;
      this._executeNext();
    };

    let setRemoteDone = () => {
      setRemoteComplete = true;
      allDone();
    };

    // If we aren't waiting for something specific, allow this
    // to complete asynchronously.
    let idpDone;
    if (!this._impl.peerIdentity) {
      idpDone = this._processIdpResult.bind(this);
      idpComplete = true; // lie about this for allDone()
    } else {
      idpDone = message => {
        let idpGood = this._processIdpResult(message);
        if (!idpGood) {
          // iff we are waiting for a very specific peerIdentity
          // call the error callback directly and then close
          idpError = "Peer Identity mismatch, expected: " +
            this._impl.peerIdentity;
          this.callCB(onError, idpError);
          this.close();
        } else {
          idpComplete = true;
          allDone();
        }
      };
    }

    try {
      this._remoteIdp.verifyIdentityFromSDP(sdp, idpDone);
    } catch (e) {
      // if processing the SDP for identity doesn't work
      this.logWarning(e.message, e.fileName, e.lineNumber);
      idpDone(null);
    }

    this._onSetRemoteDescriptionSuccess = setRemoteDone;
    this._onSetRemoteDescriptionFailure = onError;
    this._impl.setRemoteDescription(type, sdp);
  },

  setIdentityProvider: function(provider, protocol, username) {
    this._checkClosed();
    this._localIdp.setIdentityProvider(provider, protocol, username);
  },

  _gotIdentityAssertion: function(assertion){
    let args = { assertion: assertion };
    let ev = new this._win.RTCPeerConnectionIdentityEvent("identityresult", args);
    this.dispatchEvent(ev);
  },

  getIdentityAssertion: function() {
    this._checkClosed();

    var gotAssertion = assertion => {
      if (assertion) {
        this._gotIdentityAssertion(assertion);
      }
    };

    this._localIdp.getIdentityAssertion(this._impl.fingerprint,
                                        gotAssertion);
  },

  updateIce: function(config, constraints) {
    throw new this._win.DOMError("", "updateIce not yet implemented");
  },

  addIceCandidate: function(cand, onSuccess, onError) {
    if (!cand.candidate && !cand.sdpMLineIndex) {
      throw new this._win.DOMError("",
          "Invalid candidate passed to addIceCandidate!");
    }
    this._onAddIceCandidateSuccess = onSuccess || null;
    this._onAddIceCandidateError = onError || null;

    this._queueOrRun({ func: this._addIceCandidate, args: [cand], wait: true });
  },

  _addIceCandidate: function(cand) {
    this._impl.addIceCandidate(cand.candidate, cand.sdpMid || "",
                               (cand.sdpMLineIndex === null) ? 0 :
                                 cand.sdpMLineIndex + 1);
  },

  addStream: function(stream, constraints) {
    if (!constraints) {
      constraints = {};
    }
    this._mustValidateConstraints(constraints,
                                  "addStream passed invalid constraints");
    if (stream.currentTime === undefined) {
      throw new this._win.DOMError("", "Invalid stream passed to addStream!");
    }
    this._queueOrRun({ func: this._addStream,
                       args: [stream, constraints],
                       wait: false });
  },

  _addStream: function(stream, constraints) {
    this._impl.addStream(stream, constraints);
  },

  removeStream: function(stream) {
     // Bug 844295: Not implementing this functionality.
     throw new this._win.DOMError("", "removeStream not yet implemented");
  },

  getStreamById: function(id) {
    throw new this._win.DOMError("", "getStreamById not yet implemented");
  },

  close: function() {
    if (this._closed) {
      return;
    }
    this.changeIceConnectionState("closed");
    this._queueOrRun({ func: this._close, args: [false], wait: false });
    this._closed = true;
  },

  _close: function() {
    this._localIdp.close();
    this._remoteIdp.close();
    this._impl.close();
  },

  getLocalStreams: function() {
    this._checkClosed();
    return this._impl.getLocalStreams();
  },

  getRemoteStreams: function() {
    this._checkClosed();
    return this._impl.getRemoteStreams();
  },

  get localDescription() {
    this._checkClosed();
    let sdp = this._impl.localDescription;
    if (sdp.length == 0) {
      return null;
    }

    sdp = this._localIdp.wrapSdp(sdp);
    return new this._win.mozRTCSessionDescription({ type: this._localType,
                                                    sdp: sdp });
  },

  get remoteDescription() {
    this._checkClosed();
    let sdp = this._impl.remoteDescription;
    if (sdp.length == 0) {
      return null;
    }
    return new this._win.mozRTCSessionDescription({ type: this._remoteType,
                                                    sdp: sdp });
  },

  get peerIdentity() { return this._peerIdentity; },
  get id() { return this._impl.id; },
  get iceGatheringState()  { return this._iceGatheringState; },
  get iceConnectionState() { return this._iceConnectionState; },

  get signalingState() {
    // checking for our local pc closed indication
    // before invoking the pc methods.
    if (this._closed) {
      return "closed";
    }
    return {
      "SignalingInvalid":            "",
      "SignalingStable":             "stable",
      "SignalingHaveLocalOffer":     "have-local-offer",
      "SignalingHaveRemoteOffer":    "have-remote-offer",
      "SignalingHaveLocalPranswer":  "have-local-pranswer",
      "SignalingHaveRemotePranswer": "have-remote-pranswer",
      "SignalingClosed":             "closed"
    }[this._impl.signalingState];
  },

  changeIceGatheringState: function(state) {
    this._iceGatheringState = state;
    _globalPCList.notifyLifecycleObservers(this, "icegatheringstatechange");
  },

  changeIceConnectionState: function(state) {
    this._iceConnectionState = state;
    _globalPCList.notifyLifecycleObservers(this, "iceconnectionstatechange");
    this.dispatchEvent(new this._win.Event("iceconnectionstatechange"));
  },

  getStats: function(selector, onSuccess, onError) {
    this._queueOrRun({
      func: this._getStats,
      args: [selector, onSuccess, onError],
      wait: true
    });
  },

  _getStats: function(selector, onSuccess, onError) {
    this._onGetStatsSuccess = onSuccess;
    this._onGetStatsFailure = onError;

    this._impl.getStats(selector);
  },

  createDataChannel: function(label, dict) {
    this._checkClosed();
    if (dict == undefined) {
      dict = {};
    }
    if (dict.maxRetransmitNum != undefined) {
      dict.maxRetransmits = dict.maxRetransmitNum;
      this.logWarning("Deprecated RTCDataChannelInit dictionary entry maxRetransmitNum used!", null, 0);
    }
    if (dict.outOfOrderAllowed != undefined) {
      dict.ordered = !dict.outOfOrderAllowed; // the meaning is swapped with
                                              // the name change
      this.logWarning("Deprecated RTCDataChannelInit dictionary entry outOfOrderAllowed used!", null, 0);
    }
    if (dict.preset != undefined) {
      dict.negotiated = dict.preset;
      this.logWarning("Deprecated RTCDataChannelInit dictionary entry preset used!", null, 0);
    }
    if (dict.stream != undefined) {
      dict.id = dict.stream;
      this.logWarning("Deprecated RTCDataChannelInit dictionary entry stream used!", null, 0);
    }

    if (dict.maxRetransmitTime != undefined &&
        dict.maxRetransmits != undefined) {
      throw new this._win.DOMError("",
          "Both maxRetransmitTime and maxRetransmits cannot be provided");
    }
    let protocol;
    if (dict.protocol == undefined) {
      protocol = "";
    } else {
      protocol = dict.protocol;
    }

    // Must determine the type where we still know if entries are undefined.
    let type;
    if (dict.maxRetransmitTime != undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableTimed;
    } else if (dict.maxRetransmits != undefined) {
      type = Ci.IPeerConnection.kDataChannelPartialReliableRexmit;
    } else {
      type = Ci.IPeerConnection.kDataChannelReliable;
    }

    // Synchronous since it doesn't block.
    let channel = this._impl.createDataChannel(
      label, protocol, type, !dict.ordered, dict.maxRetransmitTime,
      dict.maxRetransmits, dict.negotiated ? true : false,
      dict.id != undefined ? dict.id : 0xFFFF
    );
    return channel;
  },

  connectDataConnection: function(localport, remoteport, numstreams) {
    if (numstreams == undefined || numstreams <= 0) {
      numstreams = 16;
    }
    this._queueOrRun({
      func: this._connectDataConnection,
      args: [localport, remoteport, numstreams],
      wait: false
    });
  },

  _connectDataConnection: function(localport, remoteport, numstreams) {
    this._impl.connectDataConnection(localport, remoteport, numstreams);
  }
};

function RTCError(code, message) {
  this.name = this.reasonName[Math.min(code, this.reasonName.length - 1)];
  this.message = (typeof message === "string")? message : this.name;
  this.__exposedProps__ = { name: "rw", message: "rw" };
}
RTCError.prototype = {
  // These strings must match those defined in the WebRTC spec.
  reasonName: [
    "NO_ERROR", // Should never happen -- only used for testing
    "INVALID_CONSTRAINTS_TYPE",
    "INVALID_CANDIDATE_TYPE",
    "INVALID_MEDIASTREAM_TRACK",
    "INVALID_STATE",
    "INVALID_SESSION_DESCRIPTION",
    "INCOMPATIBLE_SESSION_DESCRIPTION",
    "INCOMPATIBLE_CONSTRAINTS",
    "INCOMPATIBLE_MEDIASTREAMTRACK",
    "INTERNAL_ERROR"
  ]
};

// This is a separate object because we don't want to expose it to DOM.
function PeerConnectionObserver() {
  this._dompc = null;
}
PeerConnectionObserver.prototype = {
  classDescription: "PeerConnectionObserver",
  classID: PC_OBS_CID,
  contractID: PC_OBS_CONTRACT,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),
  init: function(win) { this._win = win; },

  __init: function(dompc) {
    this._dompc = dompc._innerObject;
  },

  dispatchEvent: function(event) {
    this._dompc.dispatchEvent(event);
  },

  onCreateOfferSuccess: function(sdp) {
    let pc = this._dompc;
    let fp = pc._impl.fingerprint;
    pc._localIdp.appendIdentityToSDP(sdp, fp, function(sdp, assertion) {
      if (assertion) {
        pc._gotIdentityAssertion(assertion);
      }
      pc.callCB(pc._onCreateOfferSuccess,
                new pc._win.mozRTCSessionDescription({ type: "offer",
                                                       sdp: sdp }));
      pc._executeNext();
    }.bind(this));
  },

  onCreateOfferError: function(code, message) {
    this._dompc.callCB(this._dompc._onCreateOfferFailure, new RTCError(code, message));
    this._dompc._executeNext();
  },

  onCreateAnswerSuccess: function(sdp) {
    let pc = this._dompc;
    let fp = pc._impl.fingerprint;
    pc._localIdp.appendIdentityToSDP(sdp, fp, function(sdp, assertion) {
      if (assertion) {
        pc._gotIdentityAssertion(assertion);
      }
      pc.callCB(pc._onCreateAnswerSuccess,
                new pc._win.mozRTCSessionDescription({ type: "answer",
                                                       sdp: sdp }));
      pc._executeNext();
    }.bind(this));
  },

  onCreateAnswerError: function(code, message) {
    this._dompc.callCB(this._dompc._onCreateAnswerFailure,
                       new RTCError(code, message));
    this._dompc._executeNext();
  },

  onSetLocalDescriptionSuccess: function() {
    this._dompc._localType = this._dompc._pendingType;
    this._dompc._pendingType = null;
    this._dompc.callCB(this._dompc._onSetLocalDescriptionSuccess);

    if (this._dompc._iceGatheringState == "complete") {
        // If we are not trickling or we completed gathering prior
        // to setLocal, then trigger a call of onicecandidate here.
        this.foundIceCandidate(null);
    }

    this._dompc._executeNext();
  },

  onSetRemoteDescriptionSuccess: function() {
    this._dompc._onSetRemoteDescriptionSuccess();
  },

  onSetLocalDescriptionError: function(code, message) {
    this._dompc._pendingType = null;
    this._dompc.callCB(this._dompc._onSetLocalDescriptionFailure,
                       new RTCError(code, message));
    this._dompc._executeNext();
  },

  onSetRemoteDescriptionError: function(code, message) {
    this._dompc._pendingType = null;
    this._dompc.callCB(this._dompc._onSetRemoteDescriptionFailure,
                       new RTCError(code, message));
    this._dompc._executeNext();
  },

  onAddIceCandidateSuccess: function() {
    this._dompc._pendingType = null;
    this._dompc.callCB(this._dompc._onAddIceCandidateSuccess);
    this._dompc._executeNext();
  },

  onAddIceCandidateError: function(code, message) {
    this._dompc._pendingType = null;
    this._dompc.callCB(this._dompc._onAddIceCandidateError,
                       new RTCError(code, message));
    this._dompc._executeNext();
  },

  onIceCandidate: function(level, mid, candidate) {
    this.foundIceCandidate(new this._dompc._win.mozRTCIceCandidate(
        {
            candidate: candidate,
            sdpMid: mid,
            sdpMLineIndex: level - 1
        }
    ));
  },


  // This method is primarily responsible for updating iceConnectionState.
  // This state is defined in the WebRTC specification as follows:
  //
  // iceConnectionState:
  // -------------------
  //   new           The ICE Agent is gathering addresses and/or waiting for
  //                 remote candidates to be supplied.
  //
  //   checking      The ICE Agent has received remote candidates on at least
  //                 one component, and is checking candidate pairs but has not
  //                 yet found a connection. In addition to checking, it may
  //                 also still be gathering.
  //
  //   connected     The ICE Agent has found a usable connection for all
  //                 components but is still checking other candidate pairs to
  //                 see if there is a better connection. It may also still be
  //                 gathering.
  //
  //   completed     The ICE Agent has finished gathering and checking and found
  //                 a connection for all components. Open issue: it is not
  //                 clear how the non controlling ICE side knows it is in the
  //                 state.
  //
  //   failed        The ICE Agent is finished checking all candidate pairs and
  //                 failed to find a connection for at least one component.
  //                 Connections may have been found for some components.
  //
  //   disconnected  Liveness checks have failed for one or more components.
  //                 This is more aggressive than failed, and may trigger
  //                 intermittently (and resolve itself without action) on a
  //                 flaky network.
  //
  //   closed        The ICE Agent has shut down and is no longer responding to
  //                 STUN requests.

  handleIceConnectionStateChange: function(iceConnectionState) {
    var histogram = Services.telemetry.getHistogramById("WEBRTC_ICE_SUCCESS_RATE");

    if (iceConnectionState === 'failed') {
      histogram.add(false);
      this._dompc.logError("ICE failed, see about:webrtc for more details", null, 0);
    }
    if (this._dompc.iceConnectionState === 'checking' &&
        (iceConnectionState === 'completed' ||
         iceConnectionState === 'connected')) {
          histogram.add(true);
    }
    this._dompc.changeIceConnectionState(iceConnectionState);
  },

  // This method is responsible for updating iceGatheringState. This
  // state is defined in the WebRTC specification as follows:
  //
  // iceGatheringState:
  // ------------------
  //   new        The object was just created, and no networking has occurred
  //              yet.
  //
  //   gathering  The ICE engine is in the process of gathering candidates for
  //              this RTCPeerConnection.
  //
  //   complete   The ICE engine has completed gathering. Events such as adding
  //              a new interface or a new TURN server will cause the state to
  //              go back to gathering.
  //
  handleIceGatheringStateChange: function(gatheringState) {
    this._dompc.changeIceGatheringState(gatheringState);

    if (gatheringState === "complete") {
      if (!this._dompc._trickleIce) {
        // If we are not trickling, then the queue is in a pending state
        // waiting for ICE gathering and executeNext frees it
        this._dompc._executeNext();
      }
      else if (this._dompc.localDescription) {
        // If we are trickling but we have already done setLocal,
        // then we need to send a final foundIceCandidate(null) to indicate
        // that we are done gathering.
        this.foundIceCandidate(null);
      }
    }
  },

  onStateChange: function(state) {
    switch (state) {
      case "SignalingState":
        this._dompc.callCB(this._dompc.onsignalingstatechange,
                           this._dompc.signalingState);
        break;

      case "IceConnectionState":
        this.handleIceConnectionStateChange(this._dompc._pc.iceConnectionState);
        break;

      case "IceGatheringState":
        this.handleIceGatheringStateChange(this._dompc._pc.iceGatheringState);
        break;

      case "SdpState":
        // No-op
        break;

      case "ReadyState":
        // No-op
        break;

      case "SipccState":
        // No-op
        break;

      default:
        this._dompc.logWarning("Unhandled state type: " + state, null, 0);
        break;
    }
  },

  onGetStatsSuccess: function(dict) {
    let chromeobj = new RTCStatsReport(this._dompc._win, dict);
    let webidlobj = this._dompc._win.RTCStatsReport._create(this._dompc._win,
                                                            chromeobj);
    chromeobj.makeStatsPublic();
    this._dompc.callCB(this._dompc._onGetStatsSuccess, webidlobj);
    this._dompc._executeNext();
  },

  onGetStatsError: function(code, message) {
    this._dompc.callCB(this._dompc._onGetStatsFailure,
                       new RTCError(code, message));
    this._dompc._executeNext();
  },

  onAddStream: function(stream) {
    let ev = new this._dompc._win.MediaStreamEvent("addstream",
                                                   { stream: stream });
    this._dompc.dispatchEvent(ev);
  },

  onRemoveStream: function(stream, type) {
    this.dispatchEvent(new this._dompc._win.MediaStreamEvent("removestream",
                                                             { stream: stream }));
  },

  foundIceCandidate: function(cand) {
    this.dispatchEvent(new this._dompc._win.RTCPeerConnectionIceEvent("icecandidate",
                                                                      { candidate: cand } ));
  },

  notifyDataChannel: function(channel) {
    this.dispatchEvent(new this._dompc._win.RTCDataChannelEvent("datachannel",
                                                                { channel: channel }));
  },

  getSupportedConstraints: function(dict) {
    return dict;
  },
};

function RTCPeerConnectionStatic() {
}
RTCPeerConnectionStatic.prototype = {
  classDescription: "mozRTCPeerConnectionStatic",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIDOMGlobalPropertyInitializer]),

  classID: PC_STATIC_CID,
  contractID: PC_STATIC_CONTRACT,

  init: function(win) {
    this._winID = win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
  },

  registerPeerConnectionLifecycleCallback: function(cb) {
    _globalPCList._registerPeerConnectionLifecycleCallback(this._winID, cb);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(
  [GlobalPCList,
   RTCIceCandidate,
   RTCSessionDescription,
   RTCPeerConnection,
   RTCPeerConnectionStatic,
   RTCStatsReport,
   RTCIdentityAssertion,
   PeerConnectionObserver]
);
