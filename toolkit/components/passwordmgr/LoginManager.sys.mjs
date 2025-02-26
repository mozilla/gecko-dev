/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PERMISSION_SAVE_LOGINS = "login-saving";
const MAX_DATE_MS = 8640000000000000;

import { LoginManagerStorage } from "resource://passwordmgr/passwordstorage.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let logger = lazy.LoginHelper.createLogger("LoginManager");
  return logger;
});

if (Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT) {
  throw new Error("LoginManager.sys.mjs should only run in the parent process");
}

export function LoginManager() {
  this.init();
}

LoginManager.prototype = {
  classID: Components.ID("{cb9e0de8-3598-4ed7-857b-827f011ad5d8}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsILoginManager",
    "nsISupportsWeakReference",
    "nsIInterfaceRequestor",
  ]),
  getInterface(aIID) {
    if (aIID.equals(Ci.mozIStorageConnection) && this._storage) {
      let ir = this._storage.QueryInterface(Ci.nsIInterfaceRequestor);
      return ir.getInterface(aIID);
    }

    if (aIID.equals(Ci.nsIVariant)) {
      // Allows unwrapping the JavaScript object for regression tests.
      return this;
    }

    throw new Components.Exception(
      "Interface not available",
      Cr.NS_ERROR_NO_INTERFACE
    );
  },

  /* ---------- private members ---------- */

  _storage: null, // Storage component which contains the saved logins

  /**
   * Initialize the Login Manager. Automatically called when service
   * is created.
   *
   * Note: Service created in BrowserGlue#_scheduleStartupIdleTasks()
   */
  init() {
    // Cache references to current |this| in utility objects
    this._observer._pwmgr = this;

    Services.obs.addObserver(this._observer, "xpcom-shutdown");
    Services.obs.addObserver(this._observer, "passwordmgr-storage-replace");

    // Initialize storage so that asynchronous data loading can start.
    this._initStorage();
  },

  _initStorage() {
    this.initializationPromise = new Promise(resolve => {
      this._storage = LoginManagerStorage.create(() => {
        resolve();

        lazy.log.debug(
          "initializationPromise is resolved, updating isPrimaryPasswordSet in sharedData"
        );
        Services.ppmm.sharedData.set(
          "isPrimaryPasswordSet",
          lazy.LoginHelper.isPrimaryPasswordSet()
        );
      });
    });
  },

  /* ---------- Utility objects ---------- */

  /**
   * Internal utility object, implements the nsIObserver interface.
   * Used to receive notification for: form submission, preference changes.
   */
  _observer: {
    _pwmgr: null,

    QueryInterface: ChromeUtils.generateQI([
      "nsIObserver",
      "nsISupportsWeakReference",
    ]),

    // nsIObserver
    observe(subject, topic, _data) {
      if (topic == "xpcom-shutdown") {
        delete this._pwmgr._storage;
        this._pwmgr = null;
      } else if (topic == "passwordmgr-storage-replace") {
        (async () => {
          await this._pwmgr._storage.terminate();
          this._pwmgr._initStorage();
          await this._pwmgr.initializationPromise;
          Services.obs.notifyObservers(
            null,
            "passwordmgr-storage-replace-complete"
          );
        })();
      } else {
        lazy.log.debug(`Unexpected notification: ${topic}.`);
      }
    },
  },

  /**
   * Ensures that a login isn't missing any necessary fields.
   *
   * @param login
   *        The login to check.
   */
  _checkLogin(login) {
    // Sanity check the login
    if (login.origin == null || !login.origin.length) {
      throw new Error("Can't add a login with a null or empty origin.");
    }

    // For logins w/o a username, set to "", not null.
    if (login.username == null) {
      throw new Error("Can't add a login with a null username.");
    }

    if (login.password == null || !login.password.length) {
      throw new Error("Can't add a login with a null or empty password.");
    }

    // Duplicated from toolkit/components/passwordmgr/LoginHelper.sys.jms
    // TODO: move all validations into this function.
    //
    // In theory these nulls should just be rolled up into the encrypted
    // values, but nsISecretDecoderRing doesn't use nsStrings, so the
    // nulls cause truncation. Check for them here just to avoid
    // unexpected round-trip surprises.
    if (login.username.includes("\0") || login.password.includes("\0")) {
      throw new Error("login values can't contain nulls");
    }

    if (login.formActionOrigin || login.formActionOrigin == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (login.httpRealm != null) {
        throw new Error(
          "Can't add a login with both a httpRealm and formActionOrigin."
        );
      }
    } else if (login.httpRealm || login.httpRealm == "") {
      // We have a HTTP realm. Can't have a form submit URL.
      if (login.formActionOrigin != null) {
        throw new Error(
          "Can't add a login with both a httpRealm and formActionOrigin."
        );
      }
    } else {
      // Need one or the other!
      throw new Error(
        "Can't add a login without a httpRealm or formActionOrigin."
      );
    }

    login.QueryInterface(Ci.nsILoginMetaInfo);
    for (let pname of ["timeCreated", "timeLastUsed", "timePasswordChanged"]) {
      // Invalid dates
      if (login[pname] > MAX_DATE_MS) {
        throw new Error("Can't add a login with invalid date properties.");
      }
    }
  },

  /* ---------- Primary Public interfaces ---------- */

  /**
   * @type Promise
   * This promise is resolved when initialization is complete, and is rejected
   * in case the asynchronous part of initialization failed.
   */
  initializationPromise: null,

  /**
   * Add a new login to login storage.
   */
  async addLoginAsync(login) {
    this._checkLogin(login);

    lazy.log.debug("Adding login");
    const [resultLogin] = await this._storage.addLoginsAsync([login]);
    return resultLogin;
  },

  /**
   * Add multiple logins to login storage.
   * TODO: rename to `addLoginsAsync` https://bugzilla.mozilla.org/show_bug.cgi?id=1832757
   */
  async addLogins(logins) {
    if (logins.length === 0) {
      return logins;
    }

    const validLogins = logins.filter(login => {
      try {
        this._checkLogin(login);
        return true;
      } catch (e) {
        console.error(e);
        return false;
      }
    });
    lazy.log.debug("Adding logins");
    return this._storage.addLoginsAsync(validLogins, true);
  },

  /**
   * Remove the specified login from the stored logins.
   */
  removeLogin(login) {
    lazy.log.debug(
      "Removing login",
      login.QueryInterface(Ci.nsILoginMetaInfo).guid
    );
    return this._storage.removeLogin(login);
  },

  /**
   * Change the specified login to match the new login or new properties.
   */
  modifyLogin(oldLogin, newLogin) {
    lazy.log.debug(
      "Modifying login",
      oldLogin.QueryInterface(Ci.nsILoginMetaInfo).guid
    );
    return this._storage.modifyLogin(oldLogin, newLogin);
  },

  /**
   * Record that the password of a saved login was used (e.g. submitted or copied).
   */
  recordPasswordUse(
    login,
    privateContextWithoutExplicitConsent,
    loginType,
    filled
  ) {
    lazy.log.debug(
      "Recording password use",
      loginType,
      login.QueryInterface(Ci.nsILoginMetaInfo).guid
    );
    if (!privateContextWithoutExplicitConsent) {
      // don't record non-interactive use in private browsing
      this._storage.recordPasswordUse(login);
    }

    Glean.pwmgr["savedLoginUsed" + loginType].record({ filled });
  },

  /**
   * Get a dump of all stored logins asynchronously. Used by the login manager UI.
   *
   * @return {nsILoginInfo[]} - If there are no logins, the array is empty.
   */
  async getAllLogins() {
    lazy.log.debug("Getting a list of all logins asynchronously.");
    return this._storage.getAllLogins();
  },

  /**
   * Get a dump of all stored logins asynchronously. Used by the login detection service.
   */
  getAllLoginsWithCallback(aCallback) {
    lazy.log.debug("Searching a list of all logins asynchronously.");
    this._storage.getAllLogins().then(logins => {
      aCallback.onSearchComplete(logins);
    });
  },

  /**
   * Remove all user facing stored logins.
   *
   * This will not remove the FxA Sync key, which is stored with the rest of a user's logins.
   */
  removeAllUserFacingLogins() {
    lazy.log.debug("Removing all user facing logins.");
    this._storage.removeAllUserFacingLogins();
  },

  /**
   * Remove all logins from data store, including the FxA Sync key.
   *
   * NOTE: You probably want `removeAllUserFacingLogins()` instead of this function.
   * This function will remove the FxA Sync key, which will break syncing of saved user data
   * e.g. bookmarks, history, open tabs, logins and passwords, add-ons, and options
   */
  removeAllLogins() {
    lazy.log.debug("Removing all logins from local store, including FxA key.");
    this._storage.removeAllLogins();
  },

  /**
   * Get a list of all origins for which logins are disabled.
   *
   * @param {Number} count - only needed for XPCOM.
   *
   * @return {String[]} of disabled origins. If there are no disabled origins,
   *                    the array is empty.
   */
  getAllDisabledHosts() {
    lazy.log.debug("Getting a list of all disabled origins.");

    let disabledHosts = [];
    for (let perm of Services.perms.all) {
      if (
        perm.type == PERMISSION_SAVE_LOGINS &&
        perm.capability == Services.perms.DENY_ACTION
      ) {
        disabledHosts.push(perm.principal.URI.displayPrePath);
      }
    }

    lazy.log.debug(`Returning ${disabledHosts.length} disabled hosts.`);
    return disabledHosts;
  },

  /**
   * Search for the known logins for entries matching the specified criteria.
   */
  findLogins(origin, formActionOrigin, httpRealm) {
    lazy.log.debug(
      "Searching for logins matching origin:",
      origin,
      "formActionOrigin:",
      formActionOrigin,
      "httpRealm:",
      httpRealm
    );

    return this._storage.findLogins(origin, formActionOrigin, httpRealm);
  },

  async searchLoginsAsync(matchData) {
    lazy.log.debug(
      `Searching for matching logins for origin: ${matchData.origin}`
    );

    if (!matchData.origin) {
      throw new Error("searchLoginsAsync: An `origin` is required");
    }

    return this._storage.searchLoginsAsync(matchData);
  },

  /**
   * @return {nsILoginInfo[]} which are decrypted.
   */
  searchLogins(matchData) {
    lazy.log.debug(
      `Searching for matching logins for origin: ${matchData.origin}`
    );

    matchData.QueryInterface(Ci.nsIPropertyBag2);
    if (!matchData.hasKey("guid")) {
      if (!matchData.hasKey("origin")) {
        lazy.log.warn("An `origin` field is recommended.");
      }
    }

    return this._storage.searchLogins(matchData);
  },

  /**
   * Search for the known logins for entries matching the specified criteria,
   * returns only the count.
   */
  countLogins(origin, formActionOrigin, httpRealm) {
    const loginsCount = this._storage.countLogins(
      origin,
      formActionOrigin,
      httpRealm
    );

    lazy.log.debug(
      `Found ${loginsCount} matching origin: ${origin}, formActionOrigin: ${formActionOrigin} and realm: ${httpRealm}`
    );

    return loginsCount;
  },

  /* Sync metadata functions */
  async getSyncID() {
    return this._storage.getSyncID();
  },

  async setSyncID(id) {
    await this._storage.setSyncID(id);
  },

  async getLastSync() {
    return this._storage.getLastSync();
  },

  async setLastSync(timestamp) {
    await this._storage.setLastSync(timestamp);
  },

  async ensureCurrentSyncID(newSyncID) {
    let existingSyncID = await this.getSyncID();
    if (existingSyncID == newSyncID) {
      return existingSyncID;
    }
    lazy.log.debug(
      `ensureCurrentSyncID: newSyncID: ${newSyncID} existingSyncID: ${existingSyncID}`
    );

    await this.setSyncID(newSyncID);
    await this.setLastSync(0);
    return newSyncID;
  },

  get uiBusy() {
    return this._storage.uiBusy;
  },

  get isLoggedIn() {
    return this._storage.isLoggedIn;
  },

  /**
   * Check to see if user has disabled saving logins for the origin.
   */
  getLoginSavingEnabled(origin) {
    lazy.log.debug(`Checking if logins to ${origin} can be saved.`);
    if (!lazy.LoginHelper.enabled) {
      return false;
    }

    try {
      let uri = Services.io.newURI(origin);
      let principal = Services.scriptSecurityManager.createContentPrincipal(
        uri,
        {}
      );
      return (
        Services.perms.testPermissionFromPrincipal(
          principal,
          PERMISSION_SAVE_LOGINS
        ) != Services.perms.DENY_ACTION
      );
    } catch (ex) {
      if (!origin.startsWith("chrome:")) {
        console.error(ex);
      }
      return false;
    }
  },

  /**
   * Enable or disable storing logins for the specified origin.
   */
  setLoginSavingEnabled(origin, enabled) {
    // Throws if there are bogus values.
    lazy.LoginHelper.checkOriginValue(origin);

    let uri = Services.io.newURI(origin);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    );
    if (enabled) {
      Services.perms.removeFromPrincipal(principal, PERMISSION_SAVE_LOGINS);
    } else {
      Services.perms.addFromPrincipal(
        principal,
        PERMISSION_SAVE_LOGINS,
        Services.perms.DENY_ACTION
      );
    }

    lazy.log.debug(
      `Enabling login saving for ${origin} now enabled? ${enabled}.`
    );
    lazy.LoginHelper.notifyStorageChanged(
      enabled ? "hostSavingEnabled" : "hostSavingDisabled",
      origin
    );
  },
}; // end of LoginManager implementation
