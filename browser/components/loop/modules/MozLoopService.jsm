/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

// Invalid auth token as per
// https://github.com/mozilla-services/loop-server/blob/45787d34108e2f0d87d74d4ddf4ff0dbab23501c/loop/errno.json#L6
const INVALID_AUTH_TOKEN = 110;

const LOOP_SESSION_TYPE = {
  GUEST: 1,
  FXA: 2
};

/***
 * Values that we segment 2-way media connection length telemetry probes
 * into.
 *
 * @type {{SHORTER_THAN_10S: Number, BETWEEN_10S_AND_30S: Number,
 *   BETWEEN_30S_AND_5M: Number, MORE_THAN_5M: Number}}
 */
const TWO_WAY_MEDIA_CONN_LENGTH = {
  SHORTER_THAN_10S: 0,
  BETWEEN_10S_AND_30S: 1,
  BETWEEN_30S_AND_5M: 2,
  MORE_THAN_5M: 3
};

/**
 * Values that we segment sharing state change telemetry probes into.
 *
 * @type {{WINDOW_ENABLED: Number, WINDOW_DISABLED: Number,
 *   BROWSER_ENABLED: Number, BROWSER_DISABLED: Number}}
 */
const SHARING_STATE_CHANGE = {
  WINDOW_ENABLED: 0,
  WINDOW_DISABLED: 1,
  BROWSER_ENABLED: 2,
  BROWSER_DISABLED: 3
};

/**
 * Values that we segment sharing a room URL action telemetry probes into.
 *
 * @type {{COPY_FROM_PANEL: Number, COPY_FROM_CONVERSATION: Number,
 *   EMAIL_FROM_CALLFAILED: Number, EMAIL_FROM_CONVERSATION: Number}}
 */
const SHARING_ROOM_URL = {
  COPY_FROM_PANEL: 0,
  COPY_FROM_CONVERSATION: 1,
  EMAIL_FROM_CALLFAILED: 2,
  EMAIL_FROM_CONVERSATION: 3
};

/**
 * Values that we segment room create action telemetry probes into.
 *
 * @type {{CREATE_SUCCESS: Number, CREATE_FAIL: Number}}
 */
const ROOM_CREATE = {
  CREATE_SUCCESS: 0,
  CREATE_FAIL: 1
};

/**
 * Values that we segment room delete action telemetry probes into.
 *
 * @type {{DELETE_SUCCESS: Number, DELETE_FAIL: Number}}
 */
const ROOM_DELETE = {
  DELETE_SUCCESS: 0,
  DELETE_FAIL: 1
};

/**
 * Values that we segment room context action telemetry probes into.
 *
 * @type {{ADD_FROM_PANEL: Number, ADD_FROM_CONVERSATION: Number}}
 */
const ROOM_CONTEXT_ADD = {
  ADD_FROM_PANEL: 0,
  ADD_FROM_CONVERSATION: 1
};

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL = "loop.debug.loglevel";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/FxAccountsOAuthClient.jsm");

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["MozLoopService", "LOOP_SESSION_TYPE",
  "TWO_WAY_MEDIA_CONN_LENGTH", "SHARING_STATE_CHANGE", "SHARING_ROOM_URL",
  "ROOM_CREATE", "ROOM_DELETE", "ROOM_CONTEXT_ADD"];

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI",
  "resource:///modules/loop/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "convertToRTCStatsReport",
  "resource://gre/modules/media/RTCStatsReport.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "loopUtils",
  "resource:///modules/loop/utils.js", "utils");
XPCOMUtils.defineLazyModuleGetter(this, "loopCrypto",
  "resource:///modules/loop/crypto.js", "LoopCrypto");

XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "CryptoUtils",
                                  "resource://services-crypto/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfileClient",
                                  "resource://gre/modules/FxAccountsProfileClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HawkClient",
                                  "resource://services-common/hawkclient.js");

XPCOMUtils.defineLazyModuleGetter(this, "deriveHawkCredentials",
                                  "resource://services-common/hawkrequest.js");

XPCOMUtils.defineLazyModuleGetter(this, "LoopContacts",
                                  "resource:///modules/loop/LoopContacts.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopStorage",
                                  "resource:///modules/loop/LoopStorage.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopCalls",
                                  "resource:///modules/loop/LoopCalls.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoopRooms",
                                  "resource:///modules/loop/LoopRooms.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "roomsPushNotification",
                                  "resource:///modules/loop/LoopRooms.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler",
                                  "resource:///modules/loop/MozLoopPushHandler.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "UITour",
                                  "resource:///modules/UITour.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");

XPCOMUtils.defineLazyServiceGetter(this, "gWM",
                                   "@mozilla.org/appshell/window-mediator;1",
                                   "nsIWindowMediator");

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "Loop"
  };
  return new ConsoleAPI(consoleOptions);
});

function setJSONPref(aName, aValue) {
  let value = !!aValue ? JSON.stringify(aValue) : "";
  Services.prefs.setCharPref(aName, value);
}

function getJSONPref(aName) {
  let value = Services.prefs.getCharPref(aName);
  return !!value ? JSON.parse(value) : null;
}

let gHawkClient = null;
let gLocalizedStrings = new Map();
let gFxAEnabled = true;
let gFxAOAuthClientPromise = null;
let gFxAOAuthClient = null;
let gErrors = new Map();
let gLastWindowId = 0;
let gConversationWindowData = new Map();

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let MozLoopServiceInternal = {
  conversationContexts: new Map(),
  pushURLs: new Map(),

  mocks: {
    pushHandler: undefined
  },

  /**
   * The current deferreds for the registration processes. This is set if in progress
   * or the registration was successful. This is null if a registration attempt was
   * unsuccessful.
   */
  deferredRegistrations: new Map(),

  get pushHandler() {
    return this.mocks.pushHandler || MozLoopPushHandler;
  },

  // The uri of the Loop server.
  get loopServerUri() {
    return Services.prefs.getCharPref("loop.server");
  },

  /**
   * The initial delay for push registration. This ensures we don't start
   * kicking off straight after browser startup, just a few seconds later.
   */
  get initialRegistrationDelayMilliseconds() {
    try {
      // Let a pref override this for developer & testing use.
      return Services.prefs.getIntPref("loop.initialDelay");
    } catch (x) {
      // Default to 5 seconds
      return 5000;
    }
  },

  /**
   * Retrieves MozLoopService Firefox Accounts OAuth token.
   *
   * @return {Object} OAuth token
   */
  get fxAOAuthTokenData() {
    return getJSONPref("loop.fxa_oauth.tokendata");
  },

  /**
   * Sets MozLoopService Firefox Accounts OAuth token.
   * If the tokenData is being cleared, will also clear the
   * profile since the profile is dependent on the token data.
   *
   * @param {Object} aTokenData OAuth token
   */
  set fxAOAuthTokenData(aTokenData) {
    setJSONPref("loop.fxa_oauth.tokendata", aTokenData);
    if (!aTokenData) {
      this.fxAOAuthProfile = null;
    }
  },

  /**
   * Sets MozLoopService Firefox Accounts Profile data.
   *
   * @param {Object} aProfileData Profile data
   */
  set fxAOAuthProfile(aProfileData) {
    setJSONPref("loop.fxa_oauth.profile", aProfileData);
    this.notifyStatusChanged(aProfileData ? "login" : undefined);
  },

  /**
   * Retrieves MozLoopService "do not disturb" pref value.
   *
   * @return {Boolean} aFlag
   */
  get doNotDisturb() {
    return Services.prefs.getBoolPref("loop.do_not_disturb");
  },

  /**
   * Sets MozLoopService "do not disturb" pref value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    Services.prefs.setBoolPref("loop.do_not_disturb", Boolean(aFlag));
    this.notifyStatusChanged();
  },

  notifyStatusChanged: function(aReason = null) {
    log.debug("notifyStatusChanged with reason:", aReason);
    let profile = MozLoopService.userProfile;
    LoopStorage.switchDatabase(profile && profile.uid);
    LoopRooms.maybeRefresh(profile && profile.uid);
    Services.obs.notifyObservers(null, "loop-status-changed", aReason);
  },

  /**
   * Record an error and notify interested UI with the relevant user-facing strings attached.
   *
   * @param {String} errorType a key to identify the type of error. Only one
   *                           error of a type will be saved at a time. This value may be used to
   *                           determine user-facing (aka. friendly) strings.
   * @param {Object} error     an object describing the error in the format from Hawk errors
   * @param {Function} [actionCallback] an object describing the label and callback function for error
   *                                    bar's button e.g. to retry.
   */
  setError: function(errorType, error, actionCallback = null) {
    log.debug("setError", errorType, error);
    log.trace();
    let messageString, detailsString, detailsButtonLabelString, detailsButtonCallback;
    const NETWORK_ERRORS = [
      Cr.NS_ERROR_CONNECTION_REFUSED,
      Cr.NS_ERROR_NET_INTERRUPT,
      Cr.NS_ERROR_NET_RESET,
      Cr.NS_ERROR_NET_TIMEOUT,
      Cr.NS_ERROR_OFFLINE,
      Cr.NS_ERROR_PROXY_CONNECTION_REFUSED,
      Cr.NS_ERROR_UNKNOWN_HOST,
      Cr.NS_ERROR_UNKNOWN_PROXY_HOST
    ];

    if (error.code === null && error.errno === null &&
        error.error instanceof Ci.nsIException &&
        NETWORK_ERRORS.indexOf(error.error.result) != -1) {
      // Network error. Override errorType so we can easily clear it on the next succesful request.
      errorType = "network";
      messageString = "could_not_connect";
      detailsString = "check_internet_connection";
      detailsButtonLabelString = "retry_button";
    } else if (errorType == "profile" && error.code >= 500 && error.code < 600) {
      messageString = "problem_accessing_account";
    } else if (error.code == 401) {
      if (errorType == "login") {
        messageString = "could_not_authenticate"; // XXX: Bug 1076377
        detailsString = "password_changed_question";
        detailsButtonLabelString = "retry_button";
        detailsButtonCallback = () => MozLoopService.logInToFxA();
      } else {
        messageString = "session_expired_error_description";
      }
    } else if (error.code >= 500 && error.code < 600) {
      messageString = "service_not_available";
      detailsString = "try_again_later";
      detailsButtonLabelString = "retry_button";
    } else {
      messageString = "generic_failure_title";
    }

    error.friendlyMessage = this.localizedStrings.get(messageString);

    // Default to the generic "retry_button" text even though the button won't be shown if
    // error.friendlyDetails is null.
    error.friendlyDetailsButtonLabel = detailsButtonLabelString ?
                                         this.localizedStrings.get(detailsButtonLabelString) :
                                         this.localizedStrings.get("retry_button");

    error.friendlyDetailsButtonCallback = actionCallback || detailsButtonCallback || null;

    if (detailsString) {
      error.friendlyDetails = this.localizedStrings.get(detailsString);
    } else if (error.friendlyDetailsButtonCallback) {
      // If we have a retry callback but no details use the generic try again string.
      error.friendlyDetails = this.localizedStrings.get("generic_failure_no_reason2");
    } else {
      error.friendlyDetails = null;
    }

    gErrors.set(errorType, error);
    this.notifyStatusChanged();
  },

  clearError: function(errorType) {
    if (gErrors.has(errorType)) {
      gErrors.delete(errorType);
      this.notifyStatusChanged();
    }
  },

  get errors() {
    return gErrors;
  },

  /**
   * Create a notification channel between the LoopServer and this client
   * via a PushServer. Once created, any subsequent changes in the pushURL
   * assigned by the PushServer will be communicated to the LoopServer.
   * with the Loop server. It will return early if already registered.
   *
   * @param {String} channelID Unique identifier for the notification channel
   *                 registered with the PushServer.
   * @param {LOOP_SESSION_TYPE} sessionType
   * @param {String} serviceType Either 'calls' or 'rooms'.
   * @param {Function} onNotification Callback function that will be associated
   *                   with this channel from the PushServer.
   * @returns {Promise} A promise that is resolved with no params on completion, or
   *                    rejected with an error code or string.
   */
  createNotificationChannel: function(channelID, sessionType, serviceType, onNotification) {
    log.debug("createNotificationChannel", channelID, sessionType, serviceType);
    // Wrap the push notification registration callback in a Promise.
    return new Promise((resolve, reject) => {
      let onRegistered = (error, pushURL, chID) => {
        log.debug("createNotificationChannel onRegistered:", error, pushURL, chID);
        if (error) {
          reject(Error(error));
        } else {
          resolve(this.registerWithLoopServer(sessionType, serviceType, pushURL));
        }
      };

      this.pushHandler.register(channelID, onRegistered, onNotification);
    });
  },

  /**
   * Starts registration of Loop with the PushServer and the LoopServer.
   * Successful PushServer registration will automatically trigger the registration
   * of the PushURL returned by the PushServer with the LoopServer. If the registration
   * chain has already been set up, this function will simply resolve.
   *
   * @param {LOOP_SESSION_TYPE} sessionType
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  promiseRegisteredWithServers: function(sessionType = LOOP_SESSION_TYPE.GUEST) {
    if (sessionType !== LOOP_SESSION_TYPE.GUEST && sessionType !== LOOP_SESSION_TYPE.FXA) {
      return Promise.reject(new Error("promiseRegisteredWithServers: Invalid sessionType"));
    }

    if (this.deferredRegistrations.has(sessionType)) {
      log.debug("promiseRegisteredWithServers: registration already completed or in progress:",
                sessionType);
      return this.deferredRegistrations.get(sessionType);
    }

    let options = this.mocks.webSocket ? { mockWebSocket: this.mocks.webSocket } : {};
    this.pushHandler.initialize(options); // This can be called more than once.

    let regPromise;
    if (sessionType == LOOP_SESSION_TYPE.GUEST) {
      regPromise = this.createNotificationChannel(
        MozLoopService.channelIDs.roomsGuest, sessionType, "rooms",
        roomsPushNotification);
    } else {
      regPromise = this.createNotificationChannel(
        MozLoopService.channelIDs.callsFxA, sessionType, "calls",
        LoopCalls.onNotification).then(() => {
          return this.createNotificationChannel(
            MozLoopService.channelIDs.roomsFxA, sessionType, "rooms",
            roomsPushNotification);
        });
    }

    log.debug("assigning to deferredRegistrations for sessionType:", sessionType);
    this.deferredRegistrations.set(sessionType, regPromise);

    // Do not return the new Promise generated by this catch() invocation.
    // This will be called along with any other onReject function attached to regPromise.
    regPromise.catch((error) => {
      log.error("Failed to register with Loop server with sessionType ", sessionType, error);
      this.deferredRegistrations.delete(sessionType);
      log.debug("Cleared deferredRegistration for sessionType:", sessionType);
    });

    return regPromise;
  },

  /**
   * Registers with the Loop server either as a guest or a FxA user.
   *
   * @private
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @param {String} serviceType: "rooms" or "calls"
   * @param {Boolean} [retry=true] Whether to retry if authentication fails.
   * @return {Promise} resolves to pushURL or rejects with an Error
   */
  registerWithLoopServer: function(sessionType, serviceType, pushURL, retry = true) {
    log.debug("registerWithLoopServer with sessionType:", sessionType, serviceType, retry);
    if (!pushURL || !sessionType || !serviceType) {
      return Promise.reject(new Error("Invalid or missing parameters for registerWithLoopServer"));
    }

    let pushURLs = this.pushURLs.get(sessionType);

    // Create a blank URL record set if none exists for this sessionType.
    if (!pushURLs) {
      pushURLs = { calls: undefined, rooms: undefined };
      this.pushURLs.set(sessionType, pushURLs);
    }

    if (pushURLs[serviceType] == pushURL) {
      return Promise.resolve(pushURL);
    }

    let newURLs = {calls: pushURLs.calls,
                   rooms: pushURLs.rooms};
    newURLs[serviceType] = pushURL;

    return this.hawkRequestInternal(sessionType, "/registration", "POST",
                                    { simplePushURLs: newURLs }).then(
      (response) => {
        // If this failed we got an invalid token.
        if (!this.storeSessionToken(sessionType, response.headers)) {
          throw new Error("session-token-wrong-size");
        }

        // Record the new push URL
        pushURLs[serviceType] = pushURL;
        log.debug("Successfully registered with server for sessionType", sessionType);
        this.clearError("registration");
        return pushURL;
      }, (error) => {
        // There's other errors than invalid auth token, but we should only do the reset
        // as a last resort.
        if (error.code === 401) {
          // Authorization failed, invalid token, we need to try again with a new token.
          // XXX (pkerr) - Why is there a retry here? This will not clear up a hawk session
          // token problem at this level.
          if (retry) {
            return this.registerWithLoopServer(sessionType, serviceType, pushURL, false);
          }
        }

        log.error("Failed to register with the loop server. Error: ", error);
        throw error;
      }
    );
  },

  /**
   * Unregisters from the Loop server either as a guest or a FxA user.
   *
   * This is normally only wanted for FxA users as we normally want to keep the
   * guest session with the device.
   *
   * NOTE: It is the responsibiliy of the caller the clear the session token
   * after all of the notification classes: calls and rooms, for either
   * Guest or FxA have been unregistered with the LoopServer.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session e.g. guest or FxA
   * @return {Promise} resolving when the unregistration request finishes
   */
  unregisterFromLoopServer: function(sessionType) {
    let prefType = Services.prefs.getPrefType(this.getSessionTokenPrefName(sessionType));
    if (prefType == Services.prefs.PREF_INVALID) {
      log.debug("already unregistered from LoopServer", sessionType);
      return Promise.resolve("already unregistered");
    }

    let error,
        pushURLs = this.pushURLs.get(sessionType),
        callsPushURL = pushURLs ? pushURLs.calls : null,
        roomsPushURL = pushURLs ? pushURLs.rooms : null;
    this.pushURLs.delete(sessionType);

    let unregister = (sessType, pushURL) => {
      if (!pushURL) {
        return Promise.resolve("no pushURL of this type to unregister");
      }

      let unregisterURL = "/registration?simplePushURL=" + encodeURIComponent(pushURL);
      return this.hawkRequestInternal(sessType, unregisterURL, "DELETE").then(
        () => {
          log.debug("Successfully unregistered from server for sessionType = ", sessType);
          return "unregistered sessionType " + sessType;
        },
        err => {
          if (err.code === 401) {
            // Authorization failed, invalid token. This is fine since it may mean we already logged out.
            log.debug("already unregistered - invalid token", sessType);
            return "already unregistered, sessionType = " + sessType;
          }

          log.error("Failed to unregister with the loop server. Error: ", error);
          throw err;
        });
    };

    return Promise.all([unregister(sessionType, callsPushURL), unregister(sessionType, roomsPushURL)]);
  },

  /**
   * Performs a hawk based request to the loop server - there is no pre-registration
   * for this request, if this is required, use hawkRequest.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        This is one of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @param {Boolean} [retryOn401=true] Whether to retry if authentication fails.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequestInternal: function(sessionType, path, method, payloadObj, retryOn401 = true) {
    log.debug("hawkRequestInternal: ", sessionType, path, method);
    if (!gHawkClient) {
      gHawkClient = new HawkClient(this.loopServerUri);
    }

    let sessionToken, credentials;
    try {
      sessionToken = Services.prefs.getCharPref(this.getSessionTokenPrefName(sessionType));
    } catch (x) {
      // It is ok for this not to exist, we'll default to sending no-creds
    }

    if (sessionToken) {
      // true = use a hex key, as required by the server (see bug 1032738).
      credentials = deriveHawkCredentials(sessionToken, "sessionToken",
                                          2 * 32, true);
    }

    if (payloadObj) {
      // Note: we must copy the object rather than mutate it, to avoid
      // mutating the values of the object passed in.
      let newPayloadObj = {};
      for (let property of Object.getOwnPropertyNames(payloadObj)) {
        if (typeof payloadObj[property] == "string") {
          newPayloadObj[property] = CommonUtils.encodeUTF8(payloadObj[property]);
        } else {
          newPayloadObj[property] = payloadObj[property];
        }
      }
      payloadObj = newPayloadObj;
    }

    let handle401Error = (error) => {
      if (sessionType === LOOP_SESSION_TYPE.FXA) {
        return MozLoopService.logOutFromFxA().then(() => {
          // Set a user-visible error after logOutFromFxA clears existing ones.
          this.setError("login", error);
          throw error;
        });
      }
      this.setError("registration", error);
      throw error;
    };

    return gHawkClient.request(path, method, credentials, payloadObj).then(
      (result) => {
        this.clearError("network");
        return result;
      },
      (error) => {
      if (error.code && error.code == 401) {
        this.clearSessionToken(sessionType);
        if (retryOn401 && sessionType === LOOP_SESSION_TYPE.GUEST) {
          log.info("401 and INVALID_AUTH_TOKEN - retry registration");
          return this.registerWithLoopServer(sessionType, false).then(
            () => {
              return this.hawkRequestInternal(sessionType, path, method, payloadObj, false);
            },
            () => {
              return handle401Error(error); //Process the original error that triggered the retry.
            }
          );
        }
        return handle401Error(error);
      }
      throw error;
    });
  },

  /**
   * Performs a hawk based request to the loop server, registering if necessary.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        This is one of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequest: function(sessionType, path, method, payloadObj) {
    log.debug("hawkRequest: " + path, sessionType);
    return new Promise((resolve, reject) => {
      MozLoopService.promiseRegisteredWithServers(sessionType).then(() => {
        this.hawkRequestInternal(sessionType, path, method, payloadObj).then(resolve, reject);
      }, err => {
        reject(err);
      }).catch(reject);
    });
  },

  /**
   * Generic hawkRequest onError handler for the hawkRequest promise.
   *
   * @param {Object} error - error reporting object
   *
   */

  _hawkRequestError: function(error) {
    log.error("Loop hawkRequest error:", error);
    throw error;
  },

  getSessionTokenPrefName: function(sessionType) {
    let suffix;
    switch (sessionType) {
      case LOOP_SESSION_TYPE.GUEST:
        suffix = "";
        break;
      case LOOP_SESSION_TYPE.FXA:
        suffix = ".fxa";
        break;
      default:
        throw new Error("Unknown LOOP_SESSION_TYPE");
    }
    return "loop.hawk-session-token" + suffix;
  },

  /**
   * Used to store a session token from a request if it exists in the headers.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   * @param {Object} headers The request headers, which may include a
   *                         "hawk-session-token" to be saved.
   * @return true on success or no token, false on failure.
   */
  storeSessionToken: function(sessionType, headers) {
    let sessionToken = headers["hawk-session-token"];
    if (sessionToken) {
      // XXX should do more validation here
      if (sessionToken.length === 64) {
        Services.prefs.setCharPref(this.getSessionTokenPrefName(sessionType), sessionToken);
        log.debug("Stored a hawk session token for sessionType", sessionType);
      } else {
        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        log.warn("Loop server sent an invalid session token");
        return false;
      }
    }
    return true;
  },

  /**
   * Clear the loop session token so we don't use it for Hawk Requests anymore.
   *
   * This should normally be used after unregistering with the server so it can
   * clean up session state first.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   */
  clearSessionToken: function(sessionType) {
    Services.prefs.clearUserPref(this.getSessionTokenPrefName(sessionType));
    log.debug("Cleared hawk session token for sessionType", sessionType);
  },

  /**
   * A getter to obtain and store the strings for loop. This is structured
   * for use by l10n.js.
   *
   * @returns {Map} a map of element ids with localized string values
   */
  get localizedStrings() {
    if (gLocalizedStrings.size) {
      return gLocalizedStrings;
    }

    let stringBundle =
      Services.strings.createBundle("chrome://browser/locale/loop/loop.properties");

    let enumerator = stringBundle.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      let string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);
      gLocalizedStrings.set(string.key, string.value);
    }
    // Supply the strings from the branding bundle on a per-need basis.
    let brandBundle =
      Services.strings.createBundle("chrome://branding/locale/brand.properties");
    // Unfortunately the `brandShortName` string is used by Loop with a lowercase 'N'.
    gLocalizedStrings.set("brandShortname", brandBundle.GetStringFromName("brandShortName"));

    return gLocalizedStrings;
  },

  /**
   * Saves loop logs to the saved-telemetry-pings folder.
   *
   * @param {Object} pc The peerConnection in question.
   */
  stageForTelemetryUpload: function(window, pc) {
    window.WebrtcGlobalInformation.getAllStats(allStats => {
      let internalFormat = allStats.reports[0]; // filtered on pc.id
      window.WebrtcGlobalInformation.getLogging("", logs => {
        let report = convertToRTCStatsReport(internalFormat);
        let logStr = "";
        logs.forEach(s => { logStr += s + "\n"; });

        // We have stats and logs.

        // Create worker job. ping = saved telemetry ping file header + payload
        //
        // Prepare payload according to https://wiki.mozilla.org/Loop/Telemetry

        let ai = Services.appinfo;
        let uuid = uuidgen.generateUUID().toString();
        uuid = uuid.substr(1, uuid.length - 2); // remove uuid curly braces

        let directory = OS.Path.join(OS.Constants.Path.profileDir,
                                     "saved-telemetry-pings");
        let job = {
          directory: directory,
          filename: uuid + ".json",
          ping: {
            reason: "loop",
            slug: uuid,
            payload: {
              ver: 1,
              info: {
                appUpdateChannel: ai.defaultUpdateChannel,
                appBuildID: ai.appBuildID,
                appName: ai.name,
                appVersion: ai.version,
                reason: "loop",
                OS: ai.OS,
                version: Services.sysinfo.getProperty("version")
              },
              report: "ice failure",
              connectionstate: pc.iceConnectionState,
              stats: report,
              localSdp: internalFormat.localSdp,
              remoteSdp: internalFormat.remoteSdp,
              log: logStr
            }
          }
        };

        // Send job to worker to do log sanitation, transcoding and saving to
        // disk for pickup by telemetry on next startup, which then uploads it.

        let worker = new ChromeWorker("MozLoopWorker.js");
        worker.onmessage = function(e) {
          log.info(e.data.ok ?
            "Successfully staged loop report for telemetry upload." :
            ("Failed to stage loop report. Error: " + e.data.fail));
        };
        worker.postMessage(job);
      });
    }, pc.id);
  },

  getChatWindowID: function(conversationWindowData) {
    // Try getting a window ID that can (re-)identify this conversation, or resort
    // to a globally unique one as a last resort.
    // XXX We can clean this up once rooms and direct contact calling are the only
    //     two modes left.
    let windowId = ("contact" in conversationWindowData) ?
                   conversationWindowData.contact._guid || gLastWindowId++ :
                   conversationWindowData.roomToken || conversationWindowData.callId ||
                   gLastWindowId++;
    return windowId.toString();
  },

  getChatURL: function(chatWindowId) {
    return "about:loopconversation#" + chatWindowId;
  },

  /**
   * Opens the chat window
   *
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @returns {Number} The id of the window, null if a window could not
   *                   be opened.
   */
  openChatWindow: function(conversationWindowData) {
    // So I guess the origin is the loop server!?
    let origin = this.loopServerUri;
    let windowId = this.getChatWindowID(conversationWindowData);

    gConversationWindowData.set(windowId, conversationWindowData);

    let url = this.getChatURL(windowId);

    let callback = chatbox => {
      // We need to use DOMContentLoaded as otherwise the injection will happen
      // in about:blank and then get lost.
      // Sadly we can't use chatbox.promiseChatLoaded() as promise chaining
      // involves event loop spins, which means it might be too late.
      // Have we already done it?
      if (chatbox.contentWindow.navigator.mozLoop) {
        return;
      }

      chatbox.addEventListener("DOMContentLoaded", function loaded(event) {
        if (event.target != chatbox.contentDocument) {
          return;
        }
        chatbox.removeEventListener("DOMContentLoaded", loaded, true);

        let chatbar = chatbox.parentNode;
        let window = chatbox.contentWindow;

        function socialFrameChanged(eventName) {
          UITour.availableTargetsCache.clear();
          UITour.notify(eventName);

          if (eventName == "Loop:ChatWindowDetached" || eventName == "Loop:ChatWindowAttached") {
            // After detach, re-attach of the chatbox, refresh its reference so
            // we can keep using it here.
            let ref = chatbar.chatboxForURL.get(chatbox.src);
            chatbox = ref && ref.get() || chatbox;
          }
        }

        window.addEventListener("socialFrameHide", socialFrameChanged.bind(null, "Loop:ChatWindowHidden"));
        window.addEventListener("socialFrameShow", socialFrameChanged.bind(null, "Loop:ChatWindowShown"));
        window.addEventListener("socialFrameDetached", socialFrameChanged.bind(null, "Loop:ChatWindowDetached"));
        window.addEventListener("socialFrameAttached", socialFrameChanged.bind(null, "Loop:ChatWindowAttached"));
        window.addEventListener("unload", socialFrameChanged.bind(null, "Loop:ChatWindowClosed"));

        const kSizeMap = {
          LoopChatEnabled: "loopChatEnabled",
          LoopChatMessageAppended: "loopChatMessageAppended"
        };

        function onChatEvent(ev) {
          // When the chat box or messages are shown, resize the panel or window
          // to be slightly higher to accomodate them.
          let customSize = kSizeMap[ev.type];
          let currSize = chatbox.getAttribute("customSize");
          // If the size is already at the requested one or at the maximum size
          // already, don't do anything. Especially don't make it shrink.
          if (customSize && currSize != customSize && currSize != "loopChatMessageAppended") {
            chatbox.setAttribute("customSize", customSize);
            chatbox.parentNode.setAttribute("customSize", customSize);
          }
        }

        window.addEventListener("LoopChatEnabled", onChatEvent);
        window.addEventListener("LoopChatMessageAppended", onChatEvent);

        injectLoopAPI(window);

        let ourID = window.QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

        let onPCLifecycleChange = (pc, winID, type) => {
          if (winID != ourID) {
            return;
          }

          // Chat Window Id, this is different that the internal winId
          let chatWindowId = window.location.hash.slice(1);
          var context = this.conversationContexts.get(chatWindowId);
          var exists = pc.id.match(/session=(\S+)/);
          if (context && !exists) {
            // Not ideal but insert our data amidst existing data like this:
            // - 000 (id=00 url=http)
            // + 000 (session=000 call=000 id=00 url=http)
            var pair = pc.id.split("(");  //)
            if (pair.length == 2) {
              pc.id = pair[0] + "(session=" + context.sessionId +
                  (context.callId ? " call=" + context.callId : "") + " " + pair[1]; //)
            }
          }

          if (type == "iceconnectionstatechange") {
            switch(pc.iceConnectionState) {
              case "failed":
              case "disconnected":
                if (Services.telemetry.canRecordExtended) {
                  this.stageForTelemetryUpload(window, pc);
                }
                break;
            }
          }
        };

        let pc_static = new window.mozRTCPeerConnectionStatic();
        pc_static.registerPeerConnectionLifecycleCallback(onPCLifecycleChange);

        UITour.notify("Loop:ChatWindowOpened");
      }.bind(this), true);
    };

    let chatboxInstance = Chat.open(null, origin, "", url, undefined, undefined,
                                    callback);
    if (!chatboxInstance) {
      return null;
    // It's common for unit tests to overload Chat.open.
    } else if (chatboxInstance.setAttribute) {
      // Set properties that influence visual appeara nce of the chatbox right
      // away to circumvent glitches.
      chatboxInstance.setAttribute("dark", true);
      chatboxInstance.setAttribute("customSize", "loopDefault");
      chatboxInstance.parentNode.setAttribute("customSize", "loopDefault");
    }
    return windowId;
  },

  /**
   * Fetch Firefox Accounts (FxA) OAuth parameters from the Loop Server.
   *
   * @return {Promise} resolved with the body of the hawk request for OAuth parameters.
   */
  promiseFxAOAuthParameters: function() {
    const SESSION_TYPE = LOOP_SESSION_TYPE.FXA;
    return this.hawkRequestInternal(SESSION_TYPE, "/fxa-oauth/params", "POST").then(response => {
      if (!this.storeSessionToken(SESSION_TYPE, response.headers)) {
        throw new Error("Invalid FxA hawk token returned");
      }
      let prefType = Services.prefs.getPrefType(this.getSessionTokenPrefName(SESSION_TYPE));
      if (prefType == Services.prefs.PREF_INVALID) {
        throw new Error("No FxA hawk token returned and we don't have one saved");
      }

      return JSON.parse(response.body);
    },
    error => { this._hawkRequestError(error); });
  },

  /**
   * Get the OAuth client constructed with Loop OAauth parameters.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise}
   */
  promiseFxAOAuthClient: Task.async(function* (forceReAuth) {
    // We must make sure to have only a single client otherwise they will have different states and
    // multiple channels. This would happen if the user clicks the Login button more than once.
    if (gFxAOAuthClientPromise) {
      return gFxAOAuthClientPromise;
    }

    gFxAOAuthClientPromise = this.promiseFxAOAuthParameters().then(
      parameters => {
        // Add the fact that we want keys to the parameters.
        parameters.keys = true;
        if (forceReAuth) {
          parameters.action = "force_auth";
          parameters.email = MozLoopService.userProfile.email;
        }

        try {
          gFxAOAuthClient = new FxAccountsOAuthClient({
            parameters: parameters
          });
        } catch (ex) {
          gFxAOAuthClientPromise = null;
          throw ex;
        }
        return gFxAOAuthClient;
      },
      error => {
        gFxAOAuthClientPromise = null;
        throw error;
      }
    );

    return gFxAOAuthClientPromise;
  }),

  /**
   * Get the OAuth client and do the authorization web flow to get an OAuth code.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise}
   */
  promiseFxAOAuthAuthorization: function(forceReAuth) {
    let deferred = Promise.defer();
    this.promiseFxAOAuthClient(forceReAuth).then(
      client => {
        client.onComplete = this._fxAOAuthComplete.bind(this, deferred);
        client.onError = this._fxAOAuthError.bind(this, deferred);
        client.launchWebFlow();
      },
      error => {
        log.error(error);
        deferred.reject(error);
      }
    );
    return deferred.promise;
  },

  /**
   * Get the OAuth token using the OAuth code and state.
   *
   * The caller should approperiately handle 4xx errors (which should lead to a logout)
   * and 5xx or connectivity issues with messaging to try again later.
   *
   * @param {String} code
   * @param {String} state
   *
   * @return {Promise} resolving with OAuth token data.
   */
  promiseFxAOAuthToken: function(code, state) {
    if (!code || !state) {
      throw new Error("promiseFxAOAuthToken: code and state are required.");
    }

    let payload = {
      code: code,
      state: state
    };
    return this.hawkRequestInternal(LOOP_SESSION_TYPE.FXA, "/fxa-oauth/token", "POST", payload).then(response => {
      return JSON.parse(response.body);
    },
    error => { this._hawkRequestError(error); });
  },

  /**
   * Called once gFxAOAuthClient fires onComplete.
   *
   * @param {Deferred} deferred used to resolve the gFxAOAuthClientPromise
   * @param {Object} result (with code and state)
   */
  _fxAOAuthComplete: function(deferred, result, keys) {
    if (keys.kBr) {
      Services.prefs.setCharPref("loop.key.fxa", keys.kBr.k);
    }
    gFxAOAuthClientPromise = null;
    // Note: The state was already verified in FxAccountsOAuthClient.
    deferred.resolve(result);
  },

  /**
   * Called if gFxAOAuthClient fires onError.
   *
   * @param {Deferred} deferred used to reject the gFxAOAuthClientPromise
   * @param {Object} error object returned by FxAOAuthClient
   */
  _fxAOAuthError: function(deferred, err) {
    gFxAOAuthClientPromise = null;
    deferred.reject(err);
  }
};
Object.freeze(MozLoopServiceInternal);


let gInitializeTimerFunc = (deferredInitialization) => {
  // Kick off the push notification service into registering after a timeout.
  // This ensures we're not doing too much straight after the browser's finished
  // starting up.

  setTimeout(MozLoopService.delayedInitialize.bind(MozLoopService, deferredInitialization),
             MozLoopServiceInternal.initialRegistrationDelayMilliseconds);
};

let gServiceInitialized = false;

/**
 * Public API
 */
this.MozLoopService = {
  _DNSService: gDNSService,
  _activeScreenShares: [],

  get channelIDs() {
    // Channel ids that will be registered with the PushServer for notifications
    return {
      callsFxA: "25389583-921f-4169-a426-a4673658944b",
      roomsFxA: "6add272a-d316-477c-8335-f00f73dfde71",
      roomsGuest: "19d3f799-a8f3-4328-9822-b7cd02765832"
    };
  },

  /**
   * Used to override the initalize timer function for test purposes.
   */
  set initializeTimerFunc(value) {
    gInitializeTimerFunc = value;
  },

  /**
   * Used to reset if the service has been initialized or not - for test
   * purposes.
   */
  resetServiceInitialized: function() {
    gServiceInitialized = false;
  },

  get roomsParticipantsCount() {
    return LoopRooms.participantsCount;
  },

  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   *
   * Note: this returns a promise for unit test purposes.
   *
   * @return {Promise}
   */
  initialize: Task.async(function*() {
    // Ensure we don't setup things like listeners more than once.
    if (gServiceInitialized) {
      return Promise.resolve();
    }

    gServiceInitialized = true;

    // Do this here, rather than immediately after definition, so that we can
    // stub out API functions for unit testing
    Object.freeze(this);

    // Don't do anything if loop is not enabled.
    if (!Services.prefs.getBoolPref("loop.enabled")) {
      return Promise.reject(new Error("loop is not enabled"));
    }

    if (Services.prefs.getPrefType("loop.fxa.enabled") == Services.prefs.PREF_BOOL) {
      gFxAEnabled = Services.prefs.getBoolPref("loop.fxa.enabled");
      if (!gFxAEnabled) {
        yield this.logOutFromFxA();
      }
    }

    // The Loop toolbar button should change icon when the room participant count
    // changes from 0 to something.
    const onRoomsChange = (e) => {
      // Pass the event name as notification reason for better logging.
      MozLoopServiceInternal.notifyStatusChanged("room-" + e);
    };
    LoopRooms.on("add", onRoomsChange);
    LoopRooms.on("update", onRoomsChange);
    LoopRooms.on("delete", onRoomsChange);
    LoopRooms.on("joined", (e, room, participant) => {
      // Don't alert if we're in the doNotDisturb mode, or the participant
      // is the owner - the content code deals with the rest of the sounds.
      if (MozLoopServiceInternal.doNotDisturb || participant.owner) {
        return;
      }

      let window = gWM.getMostRecentWindow("navigator:browser");
      if (window) {
        window.LoopUI.showNotification({
          sound: "room-joined",
          // Fallback to the brand short name if the roomName isn't available.
          title: room.roomName || MozLoopServiceInternal.localizedStrings.get("clientShortname2"),
          message: MozLoopServiceInternal.localizedStrings.get("rooms_room_joined_label"),
          selectTab: "rooms"
        });
      }
    });

    LoopRooms.on("joined", this.maybeResumeTourOnRoomJoined.bind(this));

    // If there's no guest room created and the user hasn't
    // previously authenticated then skip registration.
    if (!LoopRooms.getGuestCreatedRoom() &&
        !MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve("registration not needed");
    }

    let deferredInitialization = Promise.defer();
    gInitializeTimerFunc(deferredInitialization);

    return deferredInitialization.promise;
  }),

  /**
   * Maybe resume the tour (re-opening the tab, if necessary) if someone else joins
   * a room of ours and it's currently open.
   */
  maybeResumeTourOnRoomJoined: function(e, room, participant) {
    let isOwnerInRoom = false;
    let isOtherInRoom = false;

    if (!this.getLoopPref("gettingStarted.resumeOnFirstJoin")) {
      return;
    }

    if (!room.participants) {
      return;
    }

    // The participant that joined isn't necessarily included in room.participants (depending on
    // when the broadcast happens) so concatenate.
    for (let roomParticipant of room.participants.concat(participant)) {
      if (roomParticipant.owner) {
        isOwnerInRoom = true;
      } else {
        isOtherInRoom = true;
      }
    }

    if (!isOwnerInRoom || !isOtherInRoom) {
      return;
    }

    // Check that the room chatbox is still actually open using its URL
    let chatboxesForRoom = [...Chat.chatboxes].filter(chatbox => {
      return chatbox.src == MozLoopServiceInternal.getChatURL(room.roomToken);
    });

    if (!chatboxesForRoom.length) {
      log.warn("Tried to resume the tour from a join when the chatbox was closed", room);
      return;
    }

    this.resumeTour("open");
  },

  /**
   * The core of the initialization work that happens once the browser is ready
   * (after a timer when called during startup).
   *
   * Can be called more than once (e.g. if the initial setup fails at some phase).
   * @param {Deferred} deferredInitialization
   */
  delayedInitialize: Task.async(function*(deferredInitialization) {
    log.debug("delayedInitialize");
    // Set or clear an error depending on how deferredInitialization gets resolved.
    // We do this first so that it can handle the early returns below.
    let completedPromise = deferredInitialization.promise.then(result => {
      MozLoopServiceInternal.clearError("initialization");
      return result;
    },
    error => {
      // If we get a non-object then setError was already called for a different error type.
      if (typeof error == "object") {
        MozLoopServiceInternal.setError("initialization", error, () => MozLoopService.delayedInitialize(Promise.defer()));
      }
    });

    try {
      if (LoopRooms.getGuestCreatedRoom()) {
        yield this.promiseRegisteredWithServers(LOOP_SESSION_TYPE.GUEST);
      } else {
        log.debug("delayedInitialize: Guest Room hasn't been created so not registering as a guest");
      }
    } catch (ex) {
      log.debug("MozLoopService: Failure of guest registration", ex);
      deferredInitialization.reject(ex);
      yield completedPromise;
      return;
    }

    if (!MozLoopServiceInternal.fxAOAuthTokenData) {
      log.debug("delayedInitialize: Initialized without an already logged-in account");
      deferredInitialization.resolve("initialized without FxA status");
      yield completedPromise;
      return;
    }

    log.debug("MozLoopService: Initializing with already logged-in account");
    MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(() => {
      deferredInitialization.resolve("initialized to logged-in status");
    }, error => {
      log.debug("MozLoopService: error logging in using cached auth token");
      let retryFunc = () => MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA);
      MozLoopServiceInternal.setError("login", error, retryFunc);
      deferredInitialization.reject("error logging in using cached auth token");
    });
    yield completedPromise;
  }),

  /**
   * Opens the chat window
   *
   * @param {Object} conversationWindowData The data to be obtained by the
   *                                        window when it opens.
   * @returns {Number} The id of the window.
   */
  openChatWindow: function(conversationWindowData) {
    return MozLoopServiceInternal.openChatWindow(conversationWindowData);
  },

  /**
   * @see MozLoopServiceInternal.promiseRegisteredWithServers
   */
  promiseRegisteredWithServers: function(sessionType = LOOP_SESSION_TYPE.GUEST) {
    return MozLoopServiceInternal.promiseRegisteredWithServers(sessionType);
  },

  /**
   * Returns the strings for the specified element. Designed for use with l10n.js.
   *
   * @param {key} The element id to get strings for.
   * @return {String} A JSON string containing the localized attribute/value pairs
   *                  for the element.
   */
  getStrings: function(key) {
    var stringData = MozLoopServiceInternal.localizedStrings;
    if (!stringData.has(key)) {
      log.error("No string found for key: ", key);
      return "";
    }

    return JSON.stringify({ textContent: stringData.get(key) });
  },

  /**
   * Returns a new GUID (UUID) in curly braces format.
   */
  generateUUID: function() {
    return uuidgen.generateUUID().toString();
  },

  /**
   * Retrieves MozLoopService "do not disturb" value.
   *
   * @return {Boolean}
   */
  get doNotDisturb() {
    return MozLoopServiceInternal.doNotDisturb;
  },

  /**
   * Sets MozLoopService "do not disturb" value.
   *
   * @param {Boolean} aFlag
   */
  set doNotDisturb(aFlag) {
    MozLoopServiceInternal.doNotDisturb = aFlag;
  },

  get fxAEnabled() {
    return gFxAEnabled;
  },

  /**
   * Gets the user profile, but only if there is
   * tokenData present. Without tokenData, the
   * profile is meaningless.
   *
   * @return {Object}
   */
  get userProfile() {
    return getJSONPref("loop.fxa_oauth.tokendata") &&
           getJSONPref("loop.fxa_oauth.profile");
  },

  /**
   * Gets the encryption key for this profile.
   */
  promiseProfileEncryptionKey: function() {
    return new Promise((resolve, reject) => {
      if (this.userProfile) {
        // We're an FxA user.
        if (Services.prefs.prefHasUserValue("loop.key.fxa")) {
          resolve(MozLoopService.getLoopPref("key.fxa"));
          return;
        }

        // This should generally never happen, but its not really possible
        // for us to force reauth from here in a sensible way for the user.
        // So we'll just have to flag it the best we can.
        reject(new Error("No FxA key available"));
        return;
      }

      // XXX Temporarily save in preferences until we've got some
      // extra storage (bug 1152761).
      if (!Services.prefs.prefHasUserValue("loop.key")) {
        // Get a new value.
        loopCrypto.generateKey().then(key => {
          Services.prefs.setCharPref("loop.key", key);
          resolve(key);
        }).catch(function(error) {
          MozLoopService.log.error(error);
          reject(error);
        });
        return;
      }

      resolve(MozLoopService.getLoopPref("key"));
    });
  },

  /**
   * Returns true if this profile has an encryption key. For guest profiles
   * this is always true, since we can generate a new one if needed. For FxA
   * profiles, we need to check the preference.
   *
   * @return {Boolean} True if the profile has an encryption key.
   */
  get hasEncryptionKey() {
    return !this.userProfile ||
      Services.prefs.prefHasUserValue("loop.key.fxa");
  },

  get errors() {
    return MozLoopServiceInternal.errors;
  },

  get log() {
    return log;
  },

  /**
   * Returns the current locale
   *
   * @return {String} The code of the current locale.
   */
  get locale() {
    try {
      return Services.prefs.getComplexValue("general.useragent.locale",
        Ci.nsISupportsString).data;
    } catch (ex) {
      return "en-US";
    }
  },

  /**
   * Set any preference under "loop.".
   *
   * @param {String} prefSuffix The name of the pref without the preceding "loop."
   * @param {*} value The value to set.
   * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
   *
   * Any errors thrown by the Mozilla pref API are logged to the console.
   */
  setLoopPref: function(prefSuffix, value, prefType) {
    let prefName = "loop." + prefSuffix;
    try {
      if (!prefType) {
        prefType = Services.prefs.getPrefType(prefName);
      }
      switch (prefType) {
        case Ci.nsIPrefBranch.PREF_STRING:
          Services.prefs.setCharPref(prefName, value);
          break;
        case Ci.nsIPrefBranch.PREF_INT:
          Services.prefs.setIntPref(prefName, value);
          break;
        case Ci.nsIPrefBranch.PREF_BOOL:
          Services.prefs.setBoolPref(prefName, value);
          break;
        default:
          log.error("invalid preference type setting " + prefName);
          break;
      }
    } catch (ex) {
      log.error("setLoopPref had trouble setting " + prefName +
        "; exception: " + ex);
    }
  },

  /**
   * Return any preference under "loop.".
   *
   * @param {String} prefName The name of the pref without the preceding
   * "loop."
   * @param {Enum} prefType Type of preference, defined at Ci.nsIPrefBranch. Optional.
   *
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @return {*} on success, null on error
   */
  getLoopPref: function(prefSuffix, prefType) {
    let prefName = "loop." + prefSuffix;
    try {
      if (!prefType) {
        prefType = Services.prefs.getPrefType(prefName);
      } else if (prefType != Services.prefs.getPrefType(prefName)) {
        log.error("invalid type specified for preference");
        return null;
      }
      switch (prefType) {
        case Ci.nsIPrefBranch.PREF_STRING:
          return Services.prefs.getCharPref(prefName);
        case Ci.nsIPrefBranch.PREF_INT:
          return Services.prefs.getIntPref(prefName);
        case Ci.nsIPrefBranch.PREF_BOOL:
          return Services.prefs.getBoolPref(prefName);
        default:
          log.error("invalid preference type getting " + prefName);
          return null;
      }
    } catch (ex) {
      log.error("getLoopPref had trouble getting " + prefName +
        "; exception: " + ex);
      return null;
    }
  },

  /**
   * Start the FxA login flow using the OAuth client and params from the Loop server.
   *
   * The caller should be prepared to handle rejections related to network, server or login errors.
   *
   * @param {Boolean} forceReAuth Set to true to force the user to reauthenticate.
   * @return {Promise} that resolves when the FxA login flow is complete.
   */
  logInToFxA: function(forceReAuth) {
    log.debug("logInToFxA with fxAOAuthTokenData:", !!MozLoopServiceInternal.fxAOAuthTokenData);
    if (!forceReAuth && MozLoopServiceInternal.fxAOAuthTokenData) {
      return Promise.resolve(MozLoopServiceInternal.fxAOAuthTokenData);
    }
    return MozLoopServiceInternal.promiseFxAOAuthAuthorization(forceReAuth).then(response => {
      return MozLoopServiceInternal.promiseFxAOAuthToken(response.code, response.state);
    }).then(tokenData => {
      MozLoopServiceInternal.fxAOAuthTokenData = tokenData;
      return tokenData;
    }).then(tokenData => {
      return MozLoopServiceInternal.promiseRegisteredWithServers(LOOP_SESSION_TYPE.FXA).then(() => {
        MozLoopServiceInternal.clearError("login");
        MozLoopServiceInternal.clearError("profile");
        return MozLoopServiceInternal.fxAOAuthTokenData;
      });
    }).then(Task.async(function* fetchProfile(tokenData) {
      yield MozLoopService.fetchFxAProfile(tokenData);
      return tokenData;
    })).catch(error => {
      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);
      throw error;
    }).catch((error) => {
      MozLoopServiceInternal.setError("login", error,
                                      () => MozLoopService.logInToFxA());
      // Re-throw for testing
      throw error;
    });
  },

  /**
   * Logs the user out from FxA.
   *
   * Gracefully handles if the user is already logged out.
   *
   * @return {Promise} that resolves when the FxA logout flow is complete.
   */
  logOutFromFxA: Task.async(function*() {
    log.debug("logOutFromFxA");
    try {
      yield MozLoopServiceInternal.unregisterFromLoopServer(LOOP_SESSION_TYPE.FXA);
    }
    catch (err) {
      throw err;
    }
    finally {
      MozLoopServiceInternal.clearSessionToken(LOOP_SESSION_TYPE.FXA);
      MozLoopServiceInternal.fxAOAuthTokenData = null;
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.deferredRegistrations.delete(LOOP_SESSION_TYPE.FXA);
      // Unregister with PushHandler so these push channels will not get re-registered
      // if the connection is re-established by the PushHandler.
      MozLoopServiceInternal.pushHandler.unregister(MozLoopService.channelIDs.callsFxA);
      MozLoopServiceInternal.pushHandler.unregister(MozLoopService.channelIDs.roomsFxA);

      // Reset the client since the initial promiseFxAOAuthParameters() call is
      // what creates a new session.
      gFxAOAuthClient = null;
      gFxAOAuthClientPromise = null;

      // clearError calls notifyStatusChanged so should be done last when the
      // state is clean.
      MozLoopServiceInternal.clearError("registration");
      MozLoopServiceInternal.clearError("login");
      MozLoopServiceInternal.clearError("profile");
    }
  }),

  /**
   * Fetch/update the FxA Profile for the logged in user.
   *
   * @return {Promise} resolving if the profile information was succesfully retrieved
   *                   rejecting if the profile information couldn't be retrieved.
   *                   A profile error is registered.
   **/
  fetchFxAProfile: function() {
    log.debug("fetchFxAProfile");
    let client = new FxAccountsProfileClient({
      serverURL: gFxAOAuthClient.parameters.profile_uri,
      token: MozLoopServiceInternal.fxAOAuthTokenData.access_token
    });
    return client.fetchProfile().then(result => {
      MozLoopServiceInternal.fxAOAuthProfile = result;
      MozLoopServiceInternal.clearError("profile");
    }, error => {
      log.error("Failed to retrieve profile", error, this.fetchFxAProfile.bind(this));
      MozLoopServiceInternal.setError("profile", error);
      MozLoopServiceInternal.fxAOAuthProfile = null;
      MozLoopServiceInternal.notifyStatusChanged();
    });
  },

  openFxASettings: Task.async(function* () {
    try {
      let fxAOAuthClient = yield MozLoopServiceInternal.promiseFxAOAuthClient();
      if (!fxAOAuthClient) {
        log.error("Could not get the OAuth client");
        return;
      }
      let url = new URL("/settings", fxAOAuthClient.parameters.content_uri);
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      win.switchToTabHavingURI(url.toString(), true);
    } catch (ex) {
      log.error("Error opening FxA settings", ex);
    }
  }),

  /**
   * Gets the tour URL.
   *
   * @param {String} aSrc A string representing the entry point to begin the tour, optional.
   * @param {Object} aAdditionalParams An object with keys used as query parameter names
   */
  getTourURL: function(aSrc = null, aAdditionalParams = {}) {
    let urlStr = this.getLoopPref("gettingStarted.url");
    let url = new URL(Services.urlFormatter.formatURL(urlStr));
    for (let paramName in aAdditionalParams) {
      url.searchParams.set(paramName, aAdditionalParams[paramName]);
    }
    if (aSrc) {
      url.searchParams.set("utm_source", "firefox-browser");
      url.searchParams.set("utm_medium", "firefox-browser");
      url.searchParams.set("utm_campaign", aSrc);
    }

    // Find the most recent pageID that has the Loop prefix.
    let mostRecentLoopPageID = {id: null, lastSeen: null};
    for (let pageID of UITour.pageIDsForSession) {
      if (pageID[0] && pageID[0].startsWith("hello-tour_OpenPanel_") &&
          pageID[1] && pageID[1].lastSeen > mostRecentLoopPageID.lastSeen) {
        mostRecentLoopPageID.id = pageID[0];
        mostRecentLoopPageID.lastSeen = pageID[1].lastSeen;
      }
    }

    const PAGE_ID_EXPIRATION_MS = 60 * 60 * 1000;
    if (mostRecentLoopPageID.id &&
        mostRecentLoopPageID.lastSeen > Date.now() - PAGE_ID_EXPIRATION_MS) {
      url.searchParams.set("utm_content", mostRecentLoopPageID.id);
    }
    return url;
  },

  resumeTour: function(aIncomingConversationState) {
    if (!this.getLoopPref("gettingStarted.resumeOnFirstJoin")) {
      return;
    }

    let url = this.getTourURL("resume-with-conversation", {
      incomingConversation: aIncomingConversationState
    });

    let win = Services.wm.getMostRecentWindow("navigator:browser");

    this.setLoopPref("gettingStarted.resumeOnFirstJoin", false);

    // The query parameters of the url can vary but we always want to re-use a Loop tour tab that's
    // already open so we ignore the fragment and query string.
    let hadExistingTab = win.switchToTabHavingURI(url, true, {
      ignoreFragment: true,
      ignoreQueryString: true
    });

    // If the tab was already open, send an event instead of using the query
    // parameter above (that we don't replace on existing tabs to avoid a reload).
    if (hadExistingTab) {
      UITour.notify("Loop:IncomingConversation", {
        conversationOpen: aIncomingConversationState === "open"
      });
    }
  },

  /**
   * Opens the Getting Started tour in the browser.
   *
   * @param {String} [aSrc] A string representing the entry point to begin the tour, optional.
   */
  openGettingStartedTour: Task.async(function(aSrc = null) {
    try {
      let url = this.getTourURL(aSrc);
      let win = Services.wm.getMostRecentWindow("navigator:browser");
      win.switchToTabHavingURI(url, true, {
        ignoreFragment: true,
        replaceQueryString: true
      });
    } catch (ex) {
      log.error("Error opening Getting Started tour", ex);
    }
  }),

  /**
   * Opens a URL in a new tab in the browser.
   *
   * @param {String} url The new url to open
   */
  openURL: function(url) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    win.openUILinkIn(url, "tab");
  },

  /**
   * Performs a hawk based request to the loop server.
   *
   * @param {LOOP_SESSION_TYPE} sessionType The type of session to use for the request.
   *                                        One of the LOOP_SESSION_TYPE members.
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   * @returns {Promise}
   *        Returns a promise that resolves to the response of the API call,
   *        or is rejected with an error.  If the server response can be parsed
   *        as JSON and contains an 'error' property, the promise will be
   *        rejected with this JSON-parsed response.
   */
  hawkRequest: function(sessionType, path, method, payloadObj) {
    return MozLoopServiceInternal.hawkRequest(sessionType, path, method, payloadObj).catch(
      error => { MozLoopServiceInternal._hawkRequestError(error); });
  },

  /**
   * Returns the window data for a specific conversation window id.
   *
   * This data will be relevant to the type of window, e.g. rooms or calls.
   * See LoopRooms or LoopCalls for more information.
   *
   * @param {String} conversationWindowId
   * @returns {Object} The window data or null if error.
   */
  getConversationWindowData: function(conversationWindowId) {
    if (gConversationWindowData.has(conversationWindowId)) {
      var conversationData = gConversationWindowData.get(conversationWindowId);
      gConversationWindowData.delete(conversationWindowId);
      return conversationData;
    }

    log.error("Window data was already fetched before. Possible race condition!");
    return null;
  },

  getConversationContext: function(winId) {
    return MozLoopServiceInternal.conversationContexts.get(winId);
  },

  addConversationContext: function(windowId, context) {
    MozLoopServiceInternal.conversationContexts.set(windowId, context);
  },

  /**
   * Used to record the screen sharing state for a window so that it can
   * be reflected on the toolbar button.
   *
   * @param {String} windowId The id of the conversation window the state
   *                          is being changed for.
   * @param {Boolean} active  Whether or not screen sharing is now active.
   */
  setScreenShareState: function(windowId, active) {
    if (active) {
      this._activeScreenShares.push(windowId);
    } else {
      var index = this._activeScreenShares.indexOf(windowId);
      if (index != -1) {
        this._activeScreenShares.splice(index, 1);
      }
    }

    MozLoopServiceInternal.notifyStatusChanged();
  },

  /**
   * Returns true if screen sharing is active in at least one window.
   */
  get screenShareActive() {
    return this._activeScreenShares.length > 0;
  }
};
