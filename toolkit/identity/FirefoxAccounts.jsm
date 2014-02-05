/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FirefoxAccounts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/identity/LogUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "objectCopy",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "makeMessageObject",
                                  "resource://gre/modules/identity/IdentityUtils.jsm");

// loglevel preference should be one of: "FATAL", "ERROR", "WARN", "INFO",
// "CONFIG", "DEBUG", "TRACE" or "ALL". We will be logging error messages by
// default.
const PREF_LOG_LEVEL = "identity.fxaccounts.loglevel";
try {
  this.LOG_LEVEL =
    Services.prefs.getPrefType(PREF_LOG_LEVEL) == Ci.nsIPrefBranch.PREF_STRING
    && Services.prefs.getCharPref(PREF_LOG_LEVEL);
} catch (e) {
  this.LOG_LEVEL = Log.Level.Error;
}

let log = Log.repository.getLogger("Identity.FxAccounts");
log.level = LOG_LEVEL;
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));

#ifdef MOZ_B2G
XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsManager",
                                  "resource://gre/modules/FxAccountsManager.jsm",
                                  "FxAccountsManager");
#else
log.warn("The FxAccountsManager is only functional in B2G at this time.");
var FxAccountsManager = null;
#endif

function FxAccountsService() {
  Services.obs.addObserver(this, "quit-application-granted", false);

  // Maintain interface parity with Identity.jsm and MinimalIdentity.jsm
  this.RP = this;

  this._rpFlows = new Map();

  // Enable us to mock FxAccountsManager service in testing
  this.fxAccountsManager = FxAccountsManager;
}

FxAccountsService.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "quit-application-granted":
        Services.obs.removeObserver(this, "quit-application-granted");
        break;
    }
  },

  /**
   * Register a listener for a given windowID as a result of a call to
   * navigator.id.watch().
   *
   * @param aCaller
   *        (Object)  an object that represents the caller document, and
   *                  is expected to have properties:
   *                  - id (unique, e.g. uuid)
   *                  - origin (string)
   *
   *                  and a bunch of callbacks
   *                  - doReady()
   *                  - doLogin()
   *                  - doLogout()
   *                  - doError()
   *                  - doCancel()
   *
   */
  watch: function watch(aRpCaller) {
    this._rpFlows.set(aRpCaller.id, aRpCaller);
    log.debug("Current rp flows: " + this._rpFlows.size);

    // Nothing to do but call ready()
    let runnable = {
      run: () => {
        this.doReady(aRpCaller.id);
      }
    };
    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },

  unwatch: function(aRpCaller, aTargetMM) {
    // nothing to do
  },

  /**
   * Initiate a login with user interaction as a result of a call to
   * navigator.id.request().
   *
   * @param aRPId
   *        (integer)  the id of the doc object obtained in .watch()
   *
   * @param aOptions
   *        (Object)  options including privacyPolicy, termsOfService
   */
  request: function request(aRPId, aOptions) {
    aOptions = aOptions || {};
    let rp = this._rpFlows.get(aRPId);
    if (!rp) {
      log.error("request() called before watch()");
      return;
    }

    let options = makeMessageObject(rp);
    objectCopy(aOptions, options);

    log.debug("get assertion for " + rp.audience);

    this.fxAccountsManager.getAssertion(rp.audience).then(
      data => {
        log.debug("got assertion: " + JSON.stringify(data));
        this.doLogin(aRPId, data);
      },
      error => {
        log.error("get assertion failed: " + JSON.stringify(error));
      }
    );
  },

  /**
   * Invoked when a user wishes to logout of a site (for instance, when clicking
   * on an in-content logout button).
   *
   * @param aRpCallerId
   *        (integer)  the id of the doc object obtained in .watch()
   *
   */
  logout: function logout(aRpCallerId) {
    // XXX Bug 945363 - Resolve the SSO story for FXA and implement
    // logout accordingly.
    //
    // For now, it makes no sense to logout from a specific RP in
    // Firefox Accounts, so just directly call the logout callback.
    if (!this._rpFlows.has(aRpCallerId)) {
      log.error("logout() called before watch()");
      return;
    }

    // Call logout() on the next tick
    let runnable = {
      run: () => {
        this.doLogout(aRpCallerId);
      }
    };
    Services.tm.currentThread.dispatch(runnable,
                                       Ci.nsIThread.DISPATCH_NORMAL);
  },

  childProcessShutdown: function childProcessShutdown(messageManager) {
    for (let [key,] of this._rpFlows) {
      if (this._rpFlows.get(key)._mm === messageManager) {
        this._rpFlows.delete(key);
      }
    }
  },

  doLogin: function doLogin(aRpCallerId, aAssertion) {
    let rp = this._rpFlows.get(aRpCallerId);
    if (!rp) {
      log.warn("doLogin found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doLogin(aAssertion);
  },

  doLogout: function doLogout(aRpCallerId) {
    let rp = this._rpFlows.get(aRpCallerId);
    if (!rp) {
      log.warn("doLogout found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doLogout();
  },

  doReady: function doReady(aRpCallerId) {
    let rp = this._rpFlows.get(aRpCallerId);
    if (!rp) {
      log.warn("doReady found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doReady();
  },

  doCancel: function doCancel(aRpCallerId) {
    let rp = this._rpFlows.get(aRpCallerId);
    if (!rp) {
      log.warn("doCancel found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doCancel();
  },

  doError: function doError(aRpCallerId, aError) {
    let rp = this._rpFlows.get(aRpCallerId);
    if (!rp) {
      log.warn("doCancel found no rp to go with callerId " + aRpCallerId + "\n");
      return;
    }

    rp.doError(aError);
  }
};

this.FirefoxAccounts = new FxAccountsService();

