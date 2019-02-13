/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["LoopCalls"];

const EMAIL_OR_PHONE_RE = /^(:?\S+@\S+|\+\d+)$/;

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LOOP_SESSION_TYPE",
                                  "resource:///modules/loop/MozLoopService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopContacts",
                                  "resource:///modules/loop/LoopContacts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

 /**
 * Attempts to open a websocket.
 *
 * A new websocket interface is used each time. If an onStop callback
 * was received, calling asyncOpen() on the same interface will
 * trigger a "alreay open socket" exception even though the channel
 * is logically closed.
 */
function CallProgressSocket(progressUrl, callId, token) {
  if (!progressUrl || !callId || !token) {
    throw new Error("missing required arguments");
  }

  this._progressUrl = progressUrl;
  this._callId = callId;
  this._token = token;
}

CallProgressSocket.prototype = {
  /**
   * Open websocket and run hello exchange.
   * Sends a hello message to the server.
   *
   * @param {function} Callback used after a successful handshake
   *                   over the progressUrl.
   * @param {function} Callback used if an error is encountered
   */
  connect: function(onSuccess, onError) {
    this._onSuccess = onSuccess;
    this._onError = onError || (reason => {
      MozLoopService.log.warn("LoopCalls::callProgessSocket - ", reason);
    });

    if (!onSuccess) {
      this._onError("missing onSuccess argument");
      return;
    }

    if (Services.io.offline) {
      this._onError("IO offline");
      return;
    }

    let uri = Services.io.newURI(this._progressUrl, null, null);

    // Allow _websocket to be set for testing.
    if (!this._websocket) {
      this._websocket = Cc["@mozilla.org/network/protocol;1?name=" + uri.scheme]
                          .createInstance(Ci.nsIWebSocketChannel);

      this._websocket.initLoadInfo(null, // aLoadingNode
                                   Services.scriptSecurityManager.getSystemPrincipal(),
                                   null, // aTriggeringPrincipal
                                   Ci.nsILoadInfo.SEC_NORMAL,
                                   Ci.nsIContentPolicy.TYPE_WEBSOCKET);
    }

    this._websocket.asyncOpen(uri, this._progressUrl, this, null);
  },

  /**
   * Listener method, handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    let helloMsg = {
      messageType: "hello",
      callId: this._callId,
      auth: this._token
    };
    try { // in case websocket has closed before this handler is run
      this._websocket.sendMsg(JSON.stringify(helloMsg));
    }
    catch (error) {
      this._onError(error);
    }
  },

  /**
   * Listener method, called when the websocket is closed.
   *
   * @param {nsISupports} aContext Not used
   * @param {nsresult} aStatusCode Reason for stopping (NS_OK = successful)
   */
  onStop: function(aContext, aStatusCode) {
    if (!this._handshakeComplete) {
      this._onError("[" + aStatusCode + "]");
    }
  },

  /**
   * Listener method, called when the websocket is closed by the server.
   * If there are errors, onStop may be called without ever calling this
   * method.
   *
   * @param {nsISupports} aContext Not used
   * @param {integer} aCode the websocket closing handshake close code
   * @param {String} aReason the websocket closing handshake close reason
   */
  onServerClose: function(aContext, aCode, aReason) {
    if (!this._handshakeComplete) {
      this._onError("[" + aCode + "]" + aReason);
    }
  },

  /**
   * Listener method, called when the websocket receives a message.
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onMessageAvailable: function(aContext, aMsg) {
    let msg = {};
    try {
      msg = JSON.parse(aMsg);
    } catch (error) {
      MozLoopService.log.error("LoopCalls: error parsing progress message - ", error);
      return;
    }

    if (msg.messageType && msg.messageType === "hello") {
      this._handshakeComplete = true;
      this._onSuccess();
    }
  },


  /**
   * Create a JSON message payload and send on websocket.
   *
   * @param {Object} aMsg Message to send.
   */
  _send: function(aMsg) {
    if (!this._handshakeComplete) {
      MozLoopService.log.warn("LoopCalls::_send error - handshake not complete");
      return;
    }

    try {
      this._websocket.sendMsg(JSON.stringify(aMsg));
    }
    catch (error) {
      this._onError(error);
    }
  },

  /**
   * Notifies the server that the user has declined the call
   * with a reason of busy.
   */
  sendBusy: function() {
    this._send({
      messageType: "action",
      event: "terminate",
      reason: "busy"
    });
  }
};

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let LoopCallsInternal = {
  mocks: {
    webSocket: undefined
  },

  conversationInProgress: {},

  /**
   * Callback from MozLoopPushHandler - A push notification has been received from
   * the server.
   *
   * @param {String} version The version information from the server.
   */
  onNotification: function(version, channelID) {
    if (MozLoopService.doNotDisturb) {
      return;
    }

    // We set this here as it is assumed that once the user receives an incoming
    // call, they'll have had enough time to see the terms of service. See
    // bug 1046039 for background.
    Services.prefs.setCharPref("loop.seenToS", "seen");

    // Request the information on the new call(s) associated with this version.
    // The registered FxA session is checked first, then the anonymous session.
    // Make the call to get the GUEST session regardless of whether the FXA
    // request fails.

    if (channelID == MozLoopService.channelIDs.callsFxA && MozLoopService.userProfile) {
      this._getCalls(LOOP_SESSION_TYPE.FXA, version);
    }
  },

  /**
   * Make a hawkRequest to GET/calls?=version for this session type.
   *
   * @param {LOOP_SESSION_TYPE} sessionType - type of hawk token used
   *        for the GET operation.
   * @param {Object} version - LoopPushService notification version
   *
   * @returns {Promise}
   *
   */

  _getCalls: function(sessionType, version) {
    return MozLoopService.hawkRequest(sessionType, "/calls?version=" + version, "GET").then(
      response => { this._processCalls(response, sessionType); }
    );
  },

  /**
   * Process the calls array returned from a GET/calls?version request.
   * Only one active call is permitted at this time.
   *
   * @param {Object} response - response payload from GET
   *
   * @param {LOOP_SESSION_TYPE} sessionType - type of hawk token used
   *        for the GET operation.
   *
   */

  _processCalls: function(response, sessionType) {
    try {
      let respData = JSON.parse(response.body);
      if (respData.calls && Array.isArray(respData.calls)) {
        respData.calls.forEach((callData) => {
          if ("id" in this.conversationInProgress) {
            this._returnBusy(callData);
          } else {
            callData.sessionType = sessionType;
            callData.type = "incoming";
            this._startCall(callData);
          }
        });
      } else {
        MozLoopService.log.warn("Error: missing calls[] in response");
      }
    } catch (err) {
      MozLoopService.log.warn("Error parsing calls info", err);
    }
  },

  /**
   * Starts a call, saves the call data, and opens a chat window.
   *
   * @param {Object} callData The data associated with the call including an id.
   *                          The data should include the type - "incoming" or
   *                          "outgoing".
   */
  _startCall: function(callData) {
    const openChat = () => {
      let windowId = MozLoopService.openChatWindow(callData);
      if (windowId) {
        this.conversationInProgress.id = windowId;
      }
    };

    if (callData.type == "incoming" && ("callerId" in callData) &&
        EMAIL_OR_PHONE_RE.test(callData.callerId)) {
      LoopContacts.search({
        q: callData.callerId,
        field: callData.callerId.includes("@") ? "email" : "tel"
      }, (err, contacts) => {
        if (err) {
          // Database error, helas!
          openChat();
          return;
        }

        for (let contact of contacts) {
          if (contact.blocked) {
            // Blocked! Send a busy signal back to the caller.
            this._returnBusy(callData);
            return;
          }
        }

        openChat();
      });
    } else {
      openChat();
    }
  },

  /**
   * Starts a direct call to the contact addresses.
   *
   * @param {Object} contact The contact to call
   * @param {String} callType The type of call, e.g. "audio-video" or "audio-only"
   * @return true if the call is opened, false if it is not opened (i.e. busy)
   */
  startDirectCall: function(contact, callType) {
    if ("id" in this.conversationInProgress) {
      return false;
    }

    var callData = {
      contact: contact,
      callType: callType,
      type: "outgoing"
    };

    this._startCall(callData);
    return true;
  },

  /**
   * Block a caller so it will show up in the contacts list as a blocked contact.
   * If the contact is not yet part of the users' contacts list, it will be added
   * as a blocked contact directly.
   *
   * @param {String}   callerId Email address or phone number that may identify
   *                            the caller as an existing contact
   * @param {Function} callback Function that will be invoked once the operation
   *                            has completed. When an error occurs, it will be
   *                            passed as its first argument
   */
  blockDirectCaller: function(callerId, callback) {
    let field = callerId.contains("@") ? "email" : "tel";
    Task.spawn(function* () {
      // See if we can find the caller in our database.
      let contacts = yield LoopContacts.promise("search", {
        q: callerId,
        field: field
      });

      let contact;
      if (contacts.length) {
        for (contact of contacts) {
          yield LoopContacts.promise("block", contact._guid);
        }
      } else {
        // If the contact doesn't exist yet, add it as a blocked contact.
        contact = {
          id: MozLoopService.generateUUID(),
          name: [callerId],
          category: ["local"],
          blocked: true
        };
        // Add the phone OR email field to the contact.
        contact[field] = [{
          pref: true,
          value: callerId
        }];

        yield LoopContacts.promise("add", contact);
      }
    }).then(callback, callback);
  },

   /**
   * Open call progress websocket and terminate with a reason of busy
   * the server.
   *
   * @param {callData} Must contain the progressURL, callId and websocketToken
   *                   returned by the LoopService.
   */
  _returnBusy: function(callData) {
    let callProgress = new CallProgressSocket(
      callData.progressURL,
      callData.callId,
      callData.websocketToken);
    if (this.mocks.webSocket) {
      callProgress._websocket = this.mocks.webSocket;
    }
    // This instance of CallProgressSocket should stay alive until the underlying
    // websocket is closed since it is passed to the websocket as the nsIWebSocketListener.
    callProgress.connect(() => { callProgress.sendBusy(); });
  }
};
Object.freeze(LoopCallsInternal);

/**
 * Public API
 */
this.LoopCalls = {
  /**
   * Callback from MozLoopPushHandler - A push notification has been received from
   * the server.
   *
   * @param {String} version The version information from the server.
   */
  onNotification: function(version, channelID) {
    LoopCallsInternal.onNotification(version, channelID);
  },

  /**
   * Used to signify that a call is in progress.
   *
   * @param {String} The window id for the call in progress.
   */
  setCallInProgress: function(conversationWindowId) {
    if ("id" in LoopCallsInternal.conversationInProgress &&
        LoopCallsInternal.conversationInProgress.id != conversationWindowId) {
      MozLoopService.log.error("Starting a new conversation when one is already in progress?");
      return;
    }

    LoopCallsInternal.conversationInProgress.id = conversationWindowId;
  },

  /**
   * Releases the callData for a specific conversation window id.
   *
   * The result of this call will be a free call session slot.
   *
   * @param {Number} conversationWindowId
   */
  clearCallInProgress: function(conversationWindowId) {
    if ("id" in LoopCallsInternal.conversationInProgress &&
        LoopCallsInternal.conversationInProgress.id == conversationWindowId) {
      delete LoopCallsInternal.conversationInProgress.id;
    }
  },

    /**
     * Starts a direct call to the contact addresses.
     *
     * @param {Object} contact The contact to call
     * @param {String} callType The type of call, e.g. "audio-video" or "audio-only"
     * @return true if the call is opened, false if it is not opened (i.e. busy)
     */
  startDirectCall: function(contact, callType) {
    LoopCallsInternal.startDirectCall(contact, callType);
  },

  /**
   * @see LoopCallsInternal#blockDirectCaller
   */
  blockDirectCaller: function(callerId, callback) {
    return LoopCallsInternal.blockDirectCaller(callerId, callback);
  }
};
Object.freeze(LoopCalls);
