/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
let console = (Cu.import("resource://gre/modules/devtools/Console.jsm", {})).console;

this.EXPORTED_SYMBOLS = ["MozLoopService"];

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI",
  "resource:///modules/loop/MozLoopAPI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Chat", "resource:///modules/Chat.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "CryptoUtils",
                                  "resource://services-crypto/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "HawkClient",
                                  "resource://services-common/hawkclient.js");

XPCOMUtils.defineLazyModuleGetter(this, "deriveHawkCredentials",
                                  "resource://services-common/hawkrequest.js");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler",
                                  "resource:///modules/loop/MozLoopPushHandler.jsm");

/**
 * Internal helper methods and state
 *
 * The registration is a two-part process. First we need to connect to
 * and register with the push server. Then we need to take the result of that
 * and register with the Loop server.
 */
let MozLoopServiceInternal = {
  // The uri of the Loop server.
  loopServerUri: Services.prefs.getCharPref("loop.server"),

  // The current deferred for the registration process. This is set if in progress
  // or the registration was successful. This is null if a registration attempt was
  // unsuccessful.
  _registeredDeferred: null,

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
    return initialDelay;
  },

  /**
   * Gets the current latest expiry time for urls.
   *
   * In seconds since epoch.
   */
  get expiryTimeSeconds() {
    try {
      return Services.prefs.getIntPref("loop.urlsExpiryTimeSeconds");
    } catch (x) {
      // It is ok for the pref not to exist.
      return 0;
    }
  },

  /**
   * Sets the expiry time to either the specified time, or keeps it the same
   * depending on which is latest.
   */
  set expiryTimeSeconds(time) {
    if (time > this.expiryTimeSeconds) {
      Services.prefs.setIntPref("loop.urlsExpiryTimeSeconds", time);
    }
  },

  /**
   * Returns true if the expiry time is in the future.
   */
  urlExpiryTimeIsInFuture: function() {
    return this.expiryTimeSeconds * 1000 > Date.now();
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
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * @param {Object} mockPushHandler Optional, test-only mock push handler. Used
   *                                 to allow mocking of the MozLoopPushHandler.
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  promiseRegisteredWithServers: function(mockPushHandler) {
    if (this._registeredDeferred) {
      return this._registeredDeferred.promise;
    }

    this._registeredDeferred = Promise.defer();
    // We grab the promise early in case .initialize or its results sets
    // it back to null on error.
    let result = this._registeredDeferred.promise;

    this._pushHandler = mockPushHandler || MozLoopPushHandler;

    this._pushHandler.initialize(this.onPushRegistered.bind(this),
      this.onHandleNotification.bind(this));

    return result;
  },

  /**
   * Performs a hawk based request to the loop server.
   *
   * @param {String} path The path to make the request to.
   * @param {String} method The request method, e.g. 'POST', 'GET'.
   * @param {Object} payloadObj An object which is converted to JSON and
   *                            transmitted with the request.
   */
  hawkRequest: function(path, method, payloadObj) {
    if (!this._hawkClient) {
      this._hawkClient = new HawkClient(this.loopServerUri);
    }

    let sessionToken;
    try {
      sessionToken = Services.prefs.getCharPref("loop.hawk-session-token");
    } catch (x) {
      // It is ok for this not to exist, we'll default to sending no-creds
    }

    let credentials;
    if (sessionToken) {
      credentials = deriveHawkCredentials(sessionToken, "sessionToken", 2 * 32);
    }

    return this._hawkClient.request(path, method, credentials, payloadObj);
  },

  /**
   * Used to store a session token from a request if it exists in the headers.
   *
   * @param {Object} headers The request headers, which may include a
   *                         "hawk-session-token" to be saved.
   * @return true on success or no token, false on failure.
   */
  storeSessionToken: function(headers) {
    let sessionToken = headers["hawk-session-token"];
    if (sessionToken) {
      // XXX should do more validation here
      if (sessionToken.length === 64) {
        Services.prefs.setCharPref("loop.hawk-session-token", sessionToken);
      } else {
        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        console.warn("Loop server sent an invalid session token");
        this._registeredDeferred.reject("session-token-wrong-size");
        this._registeredDeferred = null;
        return false;
      }
    }
    return true;
  },

  /**
   * Callback from MozLoopPushHandler - The push server has been registered
   * and has given us a push url.
   *
   * @param {String} pushUrl The push url given by the push server.
   */
  onPushRegistered: function(err, pushUrl) {
    if (err) {
      this._registeredDeferred.reject(err);
      this._registeredDeferred = null;
      return;
    }

    this.registerWithLoopServer(pushUrl);
  },

  /**
   * Registers with the Loop server.
   *
   * @param {String} pushUrl The push url given by the push server.
   * @param {Boolean} noRetry Optional, don't retry if authentication fails.
   */
  registerWithLoopServer: function(pushUrl, noRetry) {
    this.hawkRequest("/registration", "POST", { simple_push_url: pushUrl})
      .then((response) => {
        // If this failed we got an invalid token. storeSessionToken rejects
        // the _registeredDeferred promise for us, so here we just need to
        // early return.
        if (!this.storeSessionToken(response.headers))
          return;

        this.registeredLoopServer = true;
        this._registeredDeferred.resolve();
        // No need to clear the promise here, everything was good, so we don't need
        // to re-register.
      }, (error) => {
        if (error.errno == 401) {
          if (this.urlExpiryTimeIsInFuture()) {
            // XXX Should this be reported to the user is a visible manner?
            Cu.reportError("Loop session token is invalid, all previously "
                           + "generated urls will no longer work.");
          }

          // Authorization failed, invalid token, we need to try again with a new token.
          Services.prefs.clearUserPref("loop.hawk-session-token");
          this.registerWithLoopServer(pushUrl, true);
          return;
        }

        // XXX Bubble the precise details up to the UI somehow (bug 1013248).
        Cu.reportError("Failed to register with the loop server. error: " + error);
        this._registeredDeferred.reject(error.errno);
        this._registeredDeferred = null;
      }
    );
  },

  /**
   * Callback from MozLoopPushHandler - A push notification has been received from
   * the server.
   *
   * @param {String} version The version information from the server.
   */
  onHandleNotification: function(version) {
    if (this.doNotDisturb) {
      return;
    }

    this.openChatWindow(null, "LooP", "about:loopconversation#incoming/" + version);
  },

  /**
   * A getter to obtain and store the strings for loop. This is structured
   * for use by l10n.js.
   *
   * @returns {Object} a map of element ids with attributes to set.
   */
  get localizedStrings() {
    if (this._localizedStrings)
      return this._localizedStrings;

    var stringBundle =
      Services.strings.createBundle('chrome://browser/locale/loop/loop.properties');

    var map = {};
    var enumerator = stringBundle.getSimpleEnumeration();
    while (enumerator.hasMoreElements()) {
      var string = enumerator.getNext().QueryInterface(Ci.nsIPropertyElement);

      // 'textContent' is the default attribute to set if none are specified.
      var key = string.key, property = 'textContent';
      var i = key.lastIndexOf('.');
      if (i >= 0) {
        property = key.substring(i + 1);
        key = key.substring(0, i);
      }
      if (!(key in map))
        map[key] = {};
      map[key][property] = string.value;
    }

    return this._localizedStrings = map;
  },

  /**
   * Opens the chat window
   *
   * @param {Object} contentWindow The window to open the chat window in, may
   *                               be null.
   * @param {String} title The title of the chat window.
   * @param {String} url The page to load in the chat window.
   * @param {String} mode May be "minimized" or undefined.
   */
  openChatWindow: function(contentWindow, title, url, mode) {
    // So I guess the origin is the loop server!?
    let origin = this.loopServerUri;
    url = url.spec || url;

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
        injectLoopAPI(chatbox.contentWindow);
      }, true);
    };

    Chat.open(contentWindow, origin, title, url, undefined, undefined, callback);
  }
};

/**
 * Public API
 */
this.MozLoopService = {
  /**
   * Initialized the loop service, and starts registration with the
   * push and loop servers.
   */
  initialize: function() {
    // If expiresTime is in the future then kick-off registration.
    if (MozLoopServiceInternal.urlExpiryTimeIsInFuture()) {
      this._startInitializeTimer();
    }
  },

  /**
   * Internal function, exposed for testing purposes only. Used to start the
   * initialize timer.
   */
  _startInitializeTimer: function() {
    // Kick off the push notification service into registering after a timeout
    // this ensures we're not doing too much straight after the browser's finished
    // starting up.
    this._initializeTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._initializeTimer.initWithCallback(function() {
      this.register();
      this._initializeTimer = null;
    }.bind(this),
    MozLoopServiceInternal.initialRegistrationDelayMilliseconds, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Starts registration of Loop with the push server, and then will register
   * with the Loop server. It will return early if already registered.
   *
   * @param {Object} mockPushHandler Optional, test-only mock push handler. Used
   *                                 to allow mocking of the MozLoopPushHandler.
   * @returns {Promise} a promise that is resolved with no params on completion, or
   *          rejected with an error code or string.
   */
  register: function(mockPushHandler) {
    return MozLoopServiceInternal.promiseRegisteredWithServers(mockPushHandler);
  },

  /**
   * Used to note a call url expiry time. If the time is later than the current
   * latest expiry time, then the stored expiry time is increased. For times
   * sooner, this function is a no-op; this ensures we always have the latest
   * expiry time for a url.
   *
   * This is used to deterimine whether or not we should be registering with the
   * push server on start.
   *
   * @param {Integer} expiryTimeSeconds The seconds since epoch of the expiry time
   *                                    of the url.
   */
  noteCallUrlExpiry: function(expiryTimeSeconds) {
    MozLoopServiceInternal.expiryTimeSeconds = expiryTimeSeconds;
  },

  /**
   * Returns the strings for the specified element. Designed for use
   * with l10n.js.
   *
   * @param {key} The element id to get strings for.
   * @return {String} A JSON string containing the localized
   *                  attribute/value pairs for the element.
   */
  getStrings: function(key) {
      var stringData = MozLoopServiceInternal.localizedStrings;
      if (!(key in stringData)) {
        Cu.reportError('No string for key: ' + key + 'found');
        return "";
      }

      return JSON.stringify(stringData[key]);
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
   * Return any preference under "loop." that's coercible to a character
   * preference.
   *
   * @param {String} prefName The name of the pref without the preceding
   * "loop."
   *
   * Any errors thrown by the Mozilla pref API are logged to the console
   * and cause null to be returned. This includes the case of the preference
   * not being found.
   *
   * @return {String} on success, null on error
   */
  getLoopCharPref: function(prefName) {
    try {
      return Services.prefs.getCharPref("loop." + prefName);
    } catch (ex) {
      console.log("getLoopCharPref had trouble getting " + prefName +
        "; exception: " + ex);
      return null;
    }
  }
};
