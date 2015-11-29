/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["fxAccounts", "FxAccounts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/FxAccountsStorage.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://services-sync/util.js");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsClient",
  "resource://gre/modules/FxAccountsClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "jwcrypto",
  "resource://gre/modules/identity/jwcrypto.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsOAuthGrantClient",
  "resource://gre/modules/FxAccountsOAuthGrantClient.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FxAccountsProfile",
  "resource://gre/modules/FxAccountsProfile.jsm");

// All properties exposed by the public FxAccounts API.
var publicProperties = [
  "accountStatus",
  "getAccountsClient",
  "getAccountsSignInURI",
  "getAccountsSignUpURI",
  "getAssertion",
  "getKeys",
  "getSignedInUser",
  "getOAuthToken",
  "getSignedInUserProfile",
  "loadAndPoll",
  "localtimeOffsetMsec",
  "now",
  "promiseAccountsForceSigninURI",
  "promiseAccountsChangeProfileURI",
  "promiseAccountsManageURI",
  "registerDeviceIfNotRegistered",
  "removeCachedOAuthToken",
  "resendVerificationEmail",
  "setSignedInUser",
  "signOut",
  "updateDevice",
  "whenVerified"
];

// An AccountState object holds all state related to one specific account.
// Only one AccountState is ever "current" in the FxAccountsInternal object -
// whenever a user logs out or logs in, the current AccountState is discarded,
// making it impossible for the wrong state or state data to be accidentally
// used.
// In addition, it has some promise-related helpers to ensure that if an
// attempt is made to resolve a promise on a "stale" state (eg, if an
// operation starts, but a different user logs in before the operation
// completes), the promise will be rejected.
// It is intended to be used thusly:
// somePromiseBasedFunction: function() {
//   let currentState = this.currentAccountState;
//   return someOtherPromiseFunction().then(
//     data => currentState.resolve(data)
//   );
// }
// If the state has changed between the function being called and the promise
// being resolved, the .resolve() call will actually be rejected.
var AccountState = this.AccountState = function(storageManager) {
  this.storageManager = storageManager;
  this.promiseInitialized = this.storageManager.getAccountData().then(data => {
    this.oauthTokens = data && data.oauthTokens ? data.oauthTokens : {};
  }).catch(err => {
    log.error("Failed to initialize the storage manager", err);
    // Things are going to fall apart, but not much we can do about it here.
  });
};

AccountState.prototype = {
  oauthTokens: null,
  whenVerifiedDeferred: null,
  whenKeysReadyDeferred: null,

  // If the storage manager has been nuked then we are no longer current.
  get isCurrent() {
    return this.storageManager != null;
  },

  abort() {
    if (this.whenVerifiedDeferred) {
      this.whenVerifiedDeferred.reject(
        new Error("Verification aborted; Another user signing in"));
      this.whenVerifiedDeferred = null;
    }

    if (this.whenKeysReadyDeferred) {
      this.whenKeysReadyDeferred.reject(
        new Error("Verification aborted; Another user signing in"));
      this.whenKeysReadyDeferred = null;
    }

    this.cert = null;
    this.keyPair = null;
    this.oauthTokens = null;
    // Avoid finalizing the storageManager multiple times (ie, .signOut()
    // followed by .abort())
    if (!this.storageManager) {
      return Promise.resolve();
    }
    let storageManager = this.storageManager;
    this.storageManager = null;
    return storageManager.finalize();
  },

  // Clobber all cached data and write that empty data to storage.
  signOut() {
    this.cert = null;
    this.keyPair = null;
    this.oauthTokens = null;
    let storageManager = this.storageManager;
    this.storageManager = null;
    return storageManager.deleteAccountData().then(() => {
      return storageManager.finalize();
    });
  },

  // Get user account data. Optionally specify explcit field names to fetch
  // (and note that if you require an in-memory field you *must* specify the
  // field name(s).)
  getUserAccountData(fieldNames = null) {
    if (!this.isCurrent) {
      return Promise.reject(new Error("Another user has signed in"));
    }
    return this.storageManager.getAccountData(fieldNames).then(result => {
      return this.resolve(result);
    });
  },

  updateUserAccountData(updatedFields) {
    if (!this.isCurrent) {
      return Promise.reject(new Error("Another user has signed in"));
    }
    return this.storageManager.updateAccountData(updatedFields);
  },

  resolve: function(result) {
    if (!this.isCurrent) {
      log.info("An accountState promise was resolved, but was actually rejected" +
               " due to a different user being signed in. Originally resolved" +
               " with", result);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.resolve(result);
  },

  reject: function(error) {
    // It could be argued that we should just let it reject with the original
    // error - but this runs the risk of the error being (eg) a 401, which
    // might cause the consumer to attempt some remediation and cause other
    // problems.
    if (!this.isCurrent) {
      log.info("An accountState promise was rejected, but we are ignoring that" +
               "reason and rejecting it due to a different user being signed in." +
               "Originally rejected with", error);
      return Promise.reject(new Error("A different user signed in"));
    }
    return Promise.reject(error);
  },

  // Abstractions for storage of cached tokens - these are all sync, and don't
  // handle revocation etc - it's just storage (and the storage itself is async,
  // but we don't return the storage promises, so it *looks* sync)
  // These functions are sync simply so we can handle "token races" - when there
  // are multiple in-flight requests for the same scope, we can detect this
  // and revoke the redundant token.

  // A preamble for the cache helpers...
  _cachePreamble() {
    if (!this.isCurrent) {
      throw new Error("Another user has signed in");
    }
  },

  // Set a cached token. |tokenData| must have a 'token' element, but may also
  // have additional fields (eg, it probably specifies the server to revoke
  // from). The 'get' functions below return the entire |tokenData| value.
  setCachedToken(scopeArray, tokenData) {
    this._cachePreamble();
    if (!tokenData.token) {
      throw new Error("No token");
    }
    let key = getScopeKey(scopeArray);
    this.oauthTokens[key] = tokenData;
    // And a background save...
    this._persistCachedTokens();
  },

  // Return data for a cached token or null (or throws on bad state etc)
  getCachedToken(scopeArray) {
    this._cachePreamble();
    let key = getScopeKey(scopeArray);
    let result = this.oauthTokens[key];
    if (result) {
      // later we might want to check an expiry date - but we currently
      // have no such concept, so just return it.
      log.trace("getCachedToken returning cached token");
      return result;
    }
    return null;
  },

  // Remove a cached token from the cache.  Does *not* revoke it from anywhere.
  // Returns the entire token entry if found, null otherwise.
  removeCachedToken(token) {
    this._cachePreamble();
    let data = this.oauthTokens;
    for (let [key, tokenValue] in Iterator(data)) {
      if (tokenValue.token == token) {
        delete data[key];
        // And a background save...
        this._persistCachedTokens();
        return tokenValue;
      }
    }
    return null;
  },

  // A hook-point for tests.  Returns a promise that's ignored in most cases
  // (notable exceptions are tests and when we explicitly are saving the entire
  // set of user data.)
  _persistCachedTokens() {
    this._cachePreamble();
    return this.updateUserAccountData({ oauthTokens: this.oauthTokens }).catch(err => {
      log.error("Failed to update cached tokens", err);
    });
  },
}

/* Given an array of scopes, make a string key by normalizing. */
function getScopeKey(scopeArray) {
  let normalizedScopes = scopeArray.map(item => item.toLowerCase());
  return normalizedScopes.sort().join("|");
}

/**
 * Copies properties from a given object to another object.
 *
 * @param from (object)
 *        The object we read property descriptors from.
 * @param to (object)
 *        The object that we set property descriptors on.
 * @param options (object) (optional)
 *        {keys: [...]}
 *          Lets the caller pass the names of all properties they want to be
 *          copied. Will copy all properties of the given source object by
 *          default.
 *        {bind: object}
 *          Lets the caller specify the object that will be used to .bind()
 *          all function properties we find to. Will bind to the given target
 *          object by default.
 */
function copyObjectProperties(from, to, opts = {}) {
  let keys = (opts && opts.keys) || Object.keys(from);
  let thisArg = (opts && opts.bind) || to;

  for (let prop of keys) {
    let desc = Object.getOwnPropertyDescriptor(from, prop);

    if (typeof(desc.value) == "function") {
      desc.value = desc.value.bind(thisArg);
    }

    if (desc.get) {
      desc.get = desc.get.bind(thisArg);
    }

    if (desc.set) {
      desc.set = desc.set.bind(thisArg);
    }

    Object.defineProperty(to, prop, desc);
  }
}

/**
 * The public API's constructor.
 */
this.FxAccounts = function (mockInternal) {
  let internal = new FxAccountsInternal();
  let external = {};

  // Copy all public properties to the 'external' object.
  let prototype = FxAccountsInternal.prototype;
  let options = {keys: publicProperties, bind: internal};
  copyObjectProperties(prototype, external, options);

  // Copy all of the mock's properties to the internal object.
  if (mockInternal && !mockInternal.onlySetInternal) {
    copyObjectProperties(mockInternal, internal);
  }

  if (mockInternal) {
    // Exposes the internal object for testing only.
    external.internal = internal;
  }

  // wait until after the mocks are setup before initializing.
  internal.initialize();

  return Object.freeze(external);
}

/**
 * The internal API's constructor.
 */
function FxAccountsInternal() {
  // Make a local copy of this constant so we can mock it in testing
  this.POLL_SESSION = POLL_SESSION;

  // All significant initialization should be done in the initialize() method
  // below as it helps with testing.
}

/**
 * The internal API's prototype.
 */
FxAccountsInternal.prototype = {
  // The timeout (in ms) we use to poll for a verified mail for the first 2 mins.
  VERIFICATION_POLL_TIMEOUT_INITIAL: 5000, // 5 seconds
  // And how often we poll after the first 2 mins.
  VERIFICATION_POLL_TIMEOUT_SUBSEQUENT: 15000, // 15 seconds.

  _fxAccountsClient: null,

  // All significant initialization should be done in this initialize() method,
  // as it's called after this object has been mocked for tests.
  initialize() {
    this.currentTimer = null;
    this.currentAccountState = this.newAccountState();
    return this.registerDeviceIfNotRegistered();
  },

  get fxAccountsClient() {
    if (!this._fxAccountsClient) {
      this._fxAccountsClient = new FxAccountsClient();
    }
    return this._fxAccountsClient;
  },

  // The profile object used to fetch the actual user profile.
  _profile: null,
  get profile() {
    if (!this._profile) {
      let profileServerUrl = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.profile.uri");
      this._profile = new FxAccountsProfile({
        fxa: this,
        profileServerUrl: profileServerUrl,
      });
    }
    return this._profile;
  },

  // A hook-point for tests who may want a mocked AccountState or mocked storage.
  newAccountState(credentials) {
    let storage = new FxAccountsStorageManager();
    storage.initialize(credentials);
    return new AccountState(storage);
  },

  /**
   * Return the current time in milliseconds as an integer.  Allows tests to
   * manipulate the date to simulate certificate expiration.
   */
  now: function() {
    return this.fxAccountsClient.now();
  },

  getAccountsClient: function() {
    return this.fxAccountsClient;
  },

  /**
   * Return clock offset in milliseconds, as reported by the fxAccountsClient.
   * This can be overridden for testing.
   *
   * The offset is the number of milliseconds that must be added to the client
   * clock to make it equal to the server clock.  For example, if the client is
   * five minutes ahead of the server, the localtimeOffsetMsec will be -300000.
   */
  get localtimeOffsetMsec() {
    return this.fxAccountsClient.localtimeOffsetMsec;
  },

  /**
   * Ask the server whether the user's email has been verified
   */
  checkEmailStatus: function checkEmailStatus(sessionToken) {
    return this.fxAccountsClient.recoveryEmailStatus(sessionToken);
  },

  /**
   * Once the user's email is verified, we can request the keys
   */
  fetchKeys: function fetchKeys(keyFetchToken) {
    log.debug("fetchKeys: " + !!keyFetchToken);
    if (logPII) {
      log.debug("fetchKeys - the token is " + keyFetchToken);
    }
    return this.fxAccountsClient.accountKeys(keyFetchToken);
  },

  // set() makes sure that polling is happening, if necessary.
  // get() does not wait for verification, and returns an object even if
  // unverified. The caller of get() must check .verified .
  // The "fxaccounts:onverified" event will fire only when the verified
  // state goes from false to true, so callers must register their observer
  // and then call get(). In particular, it will not fire when the account
  // was found to be verified in a previous boot: if our stored state says
  // the account is verified, the event will never fire. So callers must do:
  //   register notification observer (go)
  //   userdata = get()
  //   if (userdata.verified()) {go()}

  /**
   * Get the user currently signed in to Firefox Accounts.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *          verified: email verification status
   *          authAt: The time (seconds since epoch) that this record was
   *                  authenticated
   *        }
   *        or null if no user is signed in.
   */
  getSignedInUser: function getSignedInUser() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // If the email is not verified, start polling for verification,
        // but return null right away.  We don't want to return a promise
        // that might not be fulfilled for a long time.
        this.startVerifiedCheck(data);
      }
      return data;
    }).then(result => currentState.resolve(result));
  },

  /**
   * Set the current user signed in to Firefox Accounts.
   *
   * @param credentials
   *        The credentials object obtained by logging in or creating
   *        an account on the FxA server:
   *        {
   *          authAt: The time (seconds since epoch) that this record was
   *                  authenticated
   *          email: The users email address
   *          keyFetchToken: a keyFetchToken which has not yet been used
   *          sessionToken: Session for the FxA server
   *          uid: The user's unique id
   *          unwrapBKey: used to unwrap kB, derived locally from the
   *                      password (not revealed to the FxA server)
   *          verified: true/false
   *        }
   * @return Promise
   *         The promise resolves to null when the data is saved
   *         successfully and is rejected on error.
   */
  setSignedInUser: function setSignedInUser(credentials) {
    log.debug("setSignedInUser - aborting any existing flows");
    return this.abortExistingFlow().then(() => {
      let currentAccountState = this.currentAccountState = this.newAccountState(
        Cu.cloneInto(credentials, {}) // Pass a clone of the credentials object.
      );
      // This promise waits for storage, but not for verification.
      // We're telling the caller that this is durable now (although is that
      // really something we should commit to? Why not let the write happen in
      // the background? Already does for updateAccountData ;)
      return currentAccountState.promiseInitialized.then(() => {
        Services.telemetry.getHistogramById("FXA_CONFIGURED").add(1);
        this.updateDevice();
        this.notifyObservers(ONLOGIN_NOTIFICATION);
        if (!this.isUserEmailVerified(credentials)) {
          this.startVerifiedCheck(credentials);
        }
      }).then(() => {
        return currentAccountState.resolve();
      });
    })
  },


  /**
   * returns a promise that fires with the assertion.  If there is no verified
   * signed-in user, fires with null.
   */
  getAssertion: function getAssertion(audience) {
    return this._getAssertion(audience);
  },

  // getAssertion() is "public" so screws with our mock story. This
  // implementation method *can* be (and is) mocked by tests.
  _getAssertion: function _getAssertion(audience) {
    log.debug("enter getAssertion()");
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then(data => {
      if (!data) {
        // No signed-in user
        return null;
      }
      if (!this.isUserEmailVerified(data)) {
        // Signed-in user has not verified email
        return null;
      }
      if (!data.sessionToken) {
        // can't get a signed certificate without a session token, but that
        // should be impossible - make log noise about it.
        log.error("getAssertion called without a session token!");
        return null;
      }
      return this.getKeypairAndCertificate(currentState).then(
        ({keyPair, certificate}) => {
          return this.getAssertionFromCert(data, keyPair, certificate, audience);
        }
      );
    }).then(result => currentState.resolve(result));
  },

  /**
   * Resend the verification email fot the currently signed-in user.
   *
   */
  resendVerificationEmail: function resendVerificationEmail() {
    let currentState = this.currentAccountState;
    return this.getSignedInUser().then(data => {
      // If the caller is asking for verification to be re-sent, and there is
      // no signed-in user to begin with, this is probably best regarded as an
      // error.
      if (data) {
        this.pollEmailStatus(currentState, data.sessionToken, "start");
        return this.fxAccountsClient.resendVerificationEmail(data.sessionToken);
      }
      throw new Error("Cannot resend verification email; no signed-in user");
    });
  },

  /*
   * Reset state such that any previous flow is canceled.
   */
  abortExistingFlow: function abortExistingFlow() {
    if (this.currentTimer) {
      log.debug("Polling aborted; Another user signing in");
      clearTimeout(this.currentTimer);
      this.currentTimer = 0;
    }
    if (this._profile) {
      this._profile.tearDown();
      this._profile = null;
    }
    // We "abort" the accountState and assume our caller is about to throw it
    // away and replace it with a new one.
    return this.currentAccountState.abort();
  },

  accountStatus: function accountStatus() {
    return this.currentAccountState.getUserAccountData().then(data => {
      if (!data) {
        return false;
      }
      return this.fxAccountsClient.accountStatus(data.uid);
    });
  },

  _destroyOAuthToken: function(tokenData) {
    let client = new FxAccountsOAuthGrantClient({
      serverURL: tokenData.server,
      client_id: FX_OAUTH_CLIENT_ID
    });
    return client.destroyToken(tokenData.token)
  },

  _destroyAllOAuthTokens: function(tokenInfos) {
    // let's just destroy them all in parallel...
    let promises = [];
    for (let [key, tokenInfo] in Iterator(tokenInfos || {})) {
      promises.push(this._destroyOAuthToken(tokenInfo));
    }
    return Promise.all(promises);
  },

  signOut: function signOut(localOnly) {
    let currentState = this.currentAccountState;
    let sessionToken, tokensToRevoke, deviceId;
    return currentState.getUserAccountData().then(data => {
      if (data) {
        sessionToken = data.sessionToken;
        tokensToRevoke = data.oauthTokens;
        deviceId = data.deviceId;
      }
      return this._signOutLocal();
    }).then(() => {
      // FxAccountsManager calls here, then does its own call
      // to FxAccountsClient.signOut().
      if (!localOnly) {
        // Wrap this in a promise so *any* errors in signOut won't
        // block the local sign out. This is *not* returned.
        Promise.resolve().then(() => {
          // This can happen in the background and shouldn't block
          // the user from signing out. The server must tolerate
          // clients just disappearing, so this call should be best effort.
          return this._signOutServer(sessionToken, deviceId);
        }).catch(err => {
          log.error("Error during remote sign out of Firefox Accounts", err);
        }).then(() => {
          return this._destroyAllOAuthTokens(tokensToRevoke);
        }).catch(err => {
          log.error("Error during destruction of oauth tokens during signout", err);
        }).then(() => {
          // just for testing - notifications are cheap when no observers.
          this.notifyObservers("testhelper-fxa-signout-complete");
        });
      }
    }).then(() => {
      this.notifyObservers(ONLOGOUT_NOTIFICATION);
    });
  },

  /**
   * This function should be called in conjunction with a server-side
   * signOut via FxAccountsClient.
   */
  _signOutLocal: function signOutLocal() {
    let currentAccountState = this.currentAccountState;
    return currentAccountState.signOut().then(() => {
      // this "aborts" this.currentAccountState but doesn't make a new one.
      return this.abortExistingFlow();
    }).then(() => {
      this.currentAccountState = this.newAccountState();
      return this.currentAccountState.promiseInitialized;
    });
  },

  _signOutServer: function signOutServer(sessionToken, deviceId) {
    // For now we assume the service being logged out from is Sync - we might
    // need to revisit this when this FxA code is used in a context that
    // isn't Sync.
    log.debug("destroying device");
    return this.fxAccountsClient.deviceDestroy(sessionToken, deviceId, {
      service: "sync"
    });
  },

  /**
   * Fetch encryption keys for the signed-in-user from the FxA API server.
   *
   * Not for user consumption.  Exists to cause the keys to be fetch.
   *
   * Returns user data so that it can be chained with other methods.
   *
   * @return Promise
   *        The promise resolves to the credentials object of the signed-in user:
   *        {
   *          email: The user's email address
   *          uid: The user's unique id
   *          sessionToken: Session for the FxA server
   *          kA: An encryption key from the FxA server
   *          kB: An encryption key derived from the user's FxA password
   *          verified: email verification status
   *        }
   *        or null if no user is signed in
   */
  getKeys: function() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData().then((userData) => {
      if (!userData) {
        throw new Error("Can't get keys; User is not signed in");
      }
      if (userData.kA && userData.kB) {
        return userData;
      }
      if (!currentState.whenKeysReadyDeferred) {
        currentState.whenKeysReadyDeferred = Promise.defer();
        if (userData.keyFetchToken) {
          this.fetchAndUnwrapKeys(userData.keyFetchToken).then(
            (dataWithKeys) => {
              if (!dataWithKeys.kA || !dataWithKeys.kB) {
                currentState.whenKeysReadyDeferred.reject(
                  new Error("user data missing kA or kB")
                );
                return;
              }
              currentState.whenKeysReadyDeferred.resolve(dataWithKeys);
            },
            (err) => {
              currentState.whenKeysReadyDeferred.reject(err);
            }
          );
        } else {
          currentState.whenKeysReadyDeferred.reject('No keyFetchToken');
        }
      }
      return currentState.whenKeysReadyDeferred.promise;
    }).then(result => currentState.resolve(result));
   },

  fetchAndUnwrapKeys: function(keyFetchToken) {
    if (logPII) {
      log.debug("fetchAndUnwrapKeys: token: " + keyFetchToken);
    }
    let currentState = this.currentAccountState;
    return Task.spawn(function* task() {
      // Sign out if we don't have a key fetch token.
      if (!keyFetchToken) {
        log.warn("improper fetchAndUnwrapKeys() call: token missing");
        yield this.signOut();
        return null;
      }

      let {kA, wrapKB} = yield this.fetchKeys(keyFetchToken);

      let data = yield currentState.getUserAccountData();

      // Sanity check that the user hasn't changed out from under us
      if (data.keyFetchToken !== keyFetchToken) {
        throw new Error("Signed in user changed while fetching keys!");
      }

      // Next statements must be synchronous until we setUserAccountData
      // so that we don't risk getting into a weird state.
      let kB_hex = CryptoUtils.xor(CommonUtils.hexToBytes(data.unwrapBKey),
                                   wrapKB);

      if (logPII) {
        log.debug("kB_hex: " + kB_hex);
      }
      let updateData = {
        kA: CommonUtils.bytesAsHex(kA),
        kB: CommonUtils.bytesAsHex(kB_hex),
        keyFetchToken: null, // null values cause the item to be removed.
        unwrapBKey: null,
      }

      log.debug("Keys Obtained: kA=" + !!updateData.kA + ", kB=" + !!updateData.kB);
      if (logPII) {
        log.debug("Keys Obtained: kA=" + updateData.kA + ", kB=" + updateData.kB);
      }

      yield currentState.updateUserAccountData(updateData);
      // We are now ready for business. This should only be invoked once
      // per setSignedInUser(), regardless of whether we've rebooted since
      // setSignedInUser() was called.
      this.notifyObservers(ONVERIFIED_NOTIFICATION);
      return currentState.getUserAccountData();
    }.bind(this)).then(result => currentState.resolve(result));
  },

  getAssertionFromCert: function(data, keyPair, cert, audience) {
    log.debug("getAssertionFromCert");
    let payload = {};
    let d = Promise.defer();
    let options = {
      duration: ASSERTION_LIFETIME,
      localtimeOffsetMsec: this.localtimeOffsetMsec,
      now: this.now()
    };
    let currentState = this.currentAccountState;
    // "audience" should look like "http://123done.org".
    // The generated assertion will expire in two minutes.
    jwcrypto.generateAssertion(cert, keyPair, audience, options, (err, signed) => {
      if (err) {
        log.error("getAssertionFromCert: " + err);
        d.reject(err);
      } else {
        log.debug("getAssertionFromCert returning signed: " + !!signed);
        if (logPII) {
          log.debug("getAssertionFromCert returning signed: " + signed);
        }
        d.resolve(signed);
      }
    });
    return d.promise.then(result => currentState.resolve(result));
  },

  getCertificateSigned: function(sessionToken, serializedPublicKey, lifetime) {
    log.debug("getCertificateSigned: " + !!sessionToken + " " + !!serializedPublicKey);
    if (logPII) {
      log.debug("getCertificateSigned: " + sessionToken + " " + serializedPublicKey);
    }
    return this.fxAccountsClient.signCertificate(
      sessionToken,
      JSON.parse(serializedPublicKey),
      lifetime
    );
  },

  /**
   * returns a promise that fires with {keyPair, certificate}.
   */
  getKeypairAndCertificate: Task.async(function* (currentState) {
    // If the debugging pref to ignore cached authentication credentials is set for Sync,
    // then don't use any cached key pair/certificate, i.e., generate a new
    // one and get it signed.
    // The purpose of this pref is to expedite any auth errors as the result of a
    // expired or revoked FxA session token, e.g., from resetting or changing the FxA
    // password.
    let ignoreCachedAuthCredentials = false;
    try {
      ignoreCachedAuthCredentials = Services.prefs.getBoolPref("services.sync.debug.ignoreCachedAuthCredentials");
    } catch(e) {
      // Pref doesn't exist
    }
    let mustBeValidUntil = this.now() + ASSERTION_USE_PERIOD;
    let accountData = yield currentState.getUserAccountData(["cert", "keyPair", "sessionToken"]);

    let keyPairValid = !ignoreCachedAuthCredentials &&
                       accountData.keyPair &&
                       (accountData.keyPair.validUntil > mustBeValidUntil);
    let certValid = !ignoreCachedAuthCredentials &&
                    accountData.cert &&
                    (accountData.cert.validUntil > mustBeValidUntil);
    // TODO: get the lifetime from the cert's .exp field
    if (keyPairValid && certValid) {
      log.debug("getKeypairAndCertificate: already have keyPair and certificate");
      return {
        keyPair: accountData.keyPair.rawKeyPair,
        certificate: accountData.cert.rawCert
      }
    }
    // We are definately going to generate a new cert, either because it has
    // already expired, or the keyPair has - and a new keyPair means we must
    // generate a new cert.

    // A keyPair has a longer lifetime than a cert, so it's possible we will
    // have a valid keypair but an expired cert, which means we can skip
    // keypair generation.
    // Either way, the cert will require hitting the network, so bail now if
    // we know that's going to fail.
    if (Services.io.offline) {
      throw new Error(ERROR_OFFLINE);
    }

    let keyPair;
    if (keyPairValid) {
      keyPair = accountData.keyPair;
    } else {
      let keyWillBeValidUntil = this.now() + KEY_LIFETIME;
      keyPair = yield new Promise((resolve, reject) => {
        jwcrypto.generateKeyPair("DS160", (err, kp) => {
          if (err) {
            return reject(err);
          }
          log.debug("got keyPair");
          resolve({
            rawKeyPair: kp,
            validUntil: keyWillBeValidUntil,
          });
        });
      });
    }

    // and generate the cert.
    let certWillBeValidUntil = this.now() + CERT_LIFETIME;
    let certificate = yield this.getCertificateSigned(accountData.sessionToken,
                                                      keyPair.rawKeyPair.serializedPublicKey,
                                                      CERT_LIFETIME);
    log.debug("getCertificate got a new one: " + !!certificate);
    if (certificate) {
      // Cache both keypair and cert.
      let toUpdate = {
        keyPair,
        cert: {
          rawCert: certificate,
          validUntil: certWillBeValidUntil,
        },
      };
      yield currentState.updateUserAccountData(toUpdate);
    }
    return {
      keyPair: keyPair.rawKeyPair,
      certificate: certificate,
    }
  }),

  getUserAccountData: function() {
    return this.currentAccountState.getUserAccountData();
  },

  isUserEmailVerified: function isUserEmailVerified(data) {
    return !!(data && data.verified);
  },

  /**
   * Setup for and if necessary do email verification polling.
   */
  loadAndPoll: function() {
    let currentState = this.currentAccountState;
    return currentState.getUserAccountData()
      .then(data => {
        if (data) {
          Services.telemetry.getHistogramById("FXA_CONFIGURED").add(1);
          if (!this.isUserEmailVerified(data)) {
            this.pollEmailStatus(currentState, data.sessionToken, "start");
          }
        }
        return data;
      });
  },

  startVerifiedCheck: function(data) {
    log.debug("startVerifiedCheck", data && data.verified);
    if (logPII) {
      log.debug("startVerifiedCheck with user data", data);
    }

    // Get us to the verified state, then get the keys. This returns a promise
    // that will fire when we are completely ready.
    //
    // Login is truly complete once keys have been fetched, so once getKeys()
    // obtains and stores kA and kB, it will fire the onverified observer
    // notification.

    // The callers of startVerifiedCheck never consume a returned promise (ie,
    // this is simply kicking off a background fetch) so we must add a rejection
    // handler to avoid runtime warnings about the rejection not being handled.
    this.whenVerified(data).then(
      () => this.getKeys(),
      err => log.info("startVerifiedCheck promise was rejected: " + err)
    );
  },

  whenVerified: function(data) {
    let currentState = this.currentAccountState;
    if (data.verified) {
      log.debug("already verified");
      return currentState.resolve(data);
    }
    if (!currentState.whenVerifiedDeferred) {
      log.debug("whenVerified promise starts polling for verified email");
      this.pollEmailStatus(currentState, data.sessionToken, "start");
    }
    return currentState.whenVerifiedDeferred.promise.then(
      result => currentState.resolve(result)
    );
  },

  notifyObservers: function(topic, data) {
    log.debug("Notifying observers of " + topic);
    Services.obs.notifyObservers(null, topic, data);
  },

  // XXX - pollEmailStatus should maybe be on the AccountState object?
  pollEmailStatus: function pollEmailStatus(currentState, sessionToken, why) {
    log.debug("entering pollEmailStatus: " + why);
    if (why == "start") {
      if (this.currentTimer) {
        log.debug("pollEmailStatus starting while existing timer is running");
        clearTimeout(this.currentTimer);
        this.currentTimer = null;
      }

      // If we were already polling, stop and start again.  This could happen
      // if the user requested the verification email to be resent while we
      // were already polling for receipt of an earlier email.
      this.pollStartDate = Date.now();
      if (!currentState.whenVerifiedDeferred) {
        currentState.whenVerifiedDeferred = Promise.defer();
        // This deferred might not end up with any handlers (eg, if sync
        // is yet to start up.)  This might cause "A promise chain failed to
        // handle a rejection" messages, so add an error handler directly
        // on the promise to log the error.
        currentState.whenVerifiedDeferred.promise.then(null, err => {
          log.info("the wait for user verification was stopped: " + err);
        });
      }
    }

    this.checkEmailStatus(sessionToken)
      .then((response) => {
        log.debug("checkEmailStatus -> " + JSON.stringify(response));
        if (response && response.verified) {
          currentState.updateUserAccountData({ verified: true })
            .then(() => {
              return currentState.getUserAccountData();
            })
            .then(data => {
              // Now that the user is verified, we can proceed to fetch keys
              if (currentState.whenVerifiedDeferred) {
                currentState.whenVerifiedDeferred.resolve(data);
                delete currentState.whenVerifiedDeferred;
              }
              // Tell FxAccountsManager to clear its cache
              this.notifyObservers(ON_FXA_UPDATE_NOTIFICATION, ONVERIFIED_NOTIFICATION);
            });
        } else {
          // Poll email status again after a short delay.
          this.pollEmailStatusAgain(currentState, sessionToken);
        }
      }, error => {
        let timeoutMs = undefined;
        if (error && error.retryAfter) {
          // If the server told us to back off, back off the requested amount.
          timeoutMs = (error.retryAfter + 3) * 1000;
        }
        // The server will return 401 if a request parameter is erroneous or
        // if the session token expired. Let's continue polling otherwise.
        if (!error || !error.code || error.code != 401) {
          this.pollEmailStatusAgain(currentState, sessionToken, timeoutMs);
        }
      });
  },

  // Poll email status using truncated exponential back-off.
  pollEmailStatusAgain: function (currentState, sessionToken, timeoutMs) {
    let ageMs = Date.now() - this.pollStartDate;
    if (ageMs >= this.POLL_SESSION) {
      if (currentState.whenVerifiedDeferred) {
        let error = new Error("User email verification timed out.")
        currentState.whenVerifiedDeferred.reject(error);
        delete currentState.whenVerifiedDeferred;
      }
      log.debug("polling session exceeded, giving up");
      return;
    }
    if (timeoutMs === undefined) {
      let currentMinute = Math.ceil(ageMs / 60000);
      timeoutMs = currentMinute <= 2 ? this.VERIFICATION_POLL_TIMEOUT_INITIAL
                                     : this.VERIFICATION_POLL_TIMEOUT_SUBSEQUENT;
    }
    log.debug("polling with timeout = " + timeoutMs);
    this.currentTimer = setTimeout(() => {
      this.pollEmailStatus(currentState, sessionToken, "timer");
    }, timeoutMs);
  },

  _requireHttps: function() {
    let allowHttp = false;
    try {
      allowHttp = Services.prefs.getBoolPref("identity.fxaccounts.allowHttp");
    } catch(e) {
      // Pref doesn't exist
    }
    return allowHttp !== true;
  },

  // Return the URI of the remote UI flows.
  getAccountsSignUpURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signup.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Return the URI of the remote UI flows.
  getAccountsSignInURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.signin.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    return url;
  },

  // Returns a promise that resolves with the URL to use to force a re-signin
  // of the current account.
  promiseAccountsForceSigninURI: function() {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.force_auth.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    let currentState = this.currentAccountState;
    // but we need to append the email address onto a query string.
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "email=" + encodeURIComponent(accountData.email);
      return url + newQueryPortion;
    }).then(result => currentState.resolve(result));
  },

  // Returns a promise that resolves with the URL to use to change
  // the current account's profile image.
  // if settingToEdit is set, the profile page should hightlight that setting
  // for the user to edit.
  promiseAccountsChangeProfileURI: function(entrypoint, settingToEdit = null) {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.settings.uri");

    if (settingToEdit) {
      url += (url.indexOf("?") == -1 ? "?" : "&") +
             "setting=" + encodeURIComponent(settingToEdit);
    }

    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    let currentState = this.currentAccountState;
    // but we need to append the email address onto a query string.
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "email=" + encodeURIComponent(accountData.email);
      newQueryPortion += "&uid=" + encodeURIComponent(accountData.uid);
      if (entrypoint) {
        newQueryPortion += "&entrypoint=" + encodeURIComponent(entrypoint);
      }
      return url + newQueryPortion;
    }).then(result => currentState.resolve(result));
  },

  // Returns a promise that resolves with the URL to use to manage the current
  // user's FxA acct.
  promiseAccountsManageURI: function(entrypoint) {
    let url = Services.urlFormatter.formatURLPref("identity.fxaccounts.settings.uri");
    if (this._requireHttps() && !/^https:/.test(url)) { // Comment to un-break emacs js-mode highlighting
      throw new Error("Firefox Accounts server must use HTTPS");
    }
    let currentState = this.currentAccountState;
    // but we need to append the uid and email address onto a query string
    // (if the server has no matching uid it will offer to sign in with the
    // email address)
    return this.getSignedInUser().then(accountData => {
      if (!accountData) {
        return null;
      }
      let newQueryPortion = url.indexOf("?") == -1 ? "?" : "&";
      newQueryPortion += "uid=" + encodeURIComponent(accountData.uid) +
                         "&email=" + encodeURIComponent(accountData.email);
      if (entrypoint) {
        newQueryPortion += "&entrypoint=" + encodeURIComponent(entrypoint);
      }
      return url + newQueryPortion;
    }).then(result => currentState.resolve(result));
  },

  /**
   * Get an OAuth token for the user
   *
   * @param options
   *        {
   *          scope: (string/array) the oauth scope(s) being requested. As a
   *                 convenience, you may pass a string if only one scope is
   *                 required, or an array of strings if multiple are needed.
   *        }
   *
   * @return Promise.<string | Error>
   *        The promise resolves the oauth token as a string or rejects with
   *        an error object ({error: ERROR, details: {}}) of the following:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   */
  getOAuthToken: Task.async(function* (options = {}) {
    log.debug("getOAuthToken enter");
    let scope = options.scope;
    if (typeof scope === "string") {
      scope = [scope];
    }

    if (!scope || !scope.length) {
      throw this._error(ERROR_INVALID_PARAMETER, "Missing or invalid 'scope' option");
    }

    yield this._getVerifiedAccountOrReject();

    // Early exit for a cached token.
    let currentState = this.currentAccountState;
    let cached = currentState.getCachedToken(scope);
    if (cached) {
      log.debug("getOAuthToken returning a cached token");
      return cached.token;
    }

    // We are going to hit the server - this is the string we pass to it.
    let scopeString = scope.join(" ");
    let client = options.client;

    if (!client) {
      try {
        let defaultURL = Services.urlFormatter.formatURLPref("identity.fxaccounts.remote.oauth.uri");
        client = new FxAccountsOAuthGrantClient({
          serverURL: defaultURL,
          client_id: FX_OAUTH_CLIENT_ID
        });
      } catch (e) {
        throw this._error(ERROR_INVALID_PARAMETER, e);
      }
    }
    let oAuthURL = client.serverURL.href;

    try {
      log.debug("getOAuthToken fetching new token from", oAuthURL);
      let assertion = yield this.getAssertion(oAuthURL);
      let result = yield client.getTokenFromAssertion(assertion, scopeString);
      let token = result.access_token;
      // If we got one, cache it.
      if (token) {
        let entry = {token: token, server: oAuthURL};
        // But before we do, check the cache again - if we find one now, it
        // means someone else concurrently requested the same scope and beat
        // us to the cache write. To be nice to the server, we revoke the one
        // we just got and return the newly cached value.
        let cached = currentState.getCachedToken(scope);
        if (cached) {
          log.debug("Detected a race for this token - revoking the new one.");
          this._destroyOAuthToken(entry);
          return cached.token;
        }
        currentState.setCachedToken(scope, entry);
      }
      return token;
    } catch (err) {
      throw this._errorToErrorClass(err);
    }
  }),

  /**
   * Remove an OAuth token from the token cache.  Callers should call this
   * after they determine a token is invalid, so a new token will be fetched
   * on the next call to getOAuthToken().
   *
   * @param options
   *        {
   *          token: (string) A previously fetched token.
   *        }
   * @return Promise.<undefined> This function will always resolve, even if
   *         an unknown token is passed.
   */
   removeCachedOAuthToken: Task.async(function* (options) {
    if (!options.token || typeof options.token !== "string") {
      throw this._error(ERROR_INVALID_PARAMETER, "Missing or invalid 'token' option");
    }
    let currentState = this.currentAccountState;
    let existing = currentState.removeCachedToken(options.token);
    if (existing) {
      // background destroy.
      this._destroyOAuthToken(existing).catch(err => {
        log.warn("FxA failed to revoke a cached token", err);
      });
    }
   }),

  _getVerifiedAccountOrReject: Task.async(function* () {
    let data = yield this.currentAccountState.getUserAccountData();
    if (!data) {
      // No signed-in user
      throw this._error(ERROR_NO_ACCOUNT);
    }
    if (!this.isUserEmailVerified(data)) {
      // Signed-in user has not verified email
      throw this._error(ERROR_UNVERIFIED_ACCOUNT);
    }
  }),

  /*
   * Coerce an error into one of the general error cases:
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   *
   * These errors will pass through:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   */
  _errorToErrorClass: function (aError) {
    if (aError.errno) {
      let error = SERVER_ERRNO_TO_ERROR[aError.errno];
      return this._error(ERROR_TO_GENERAL_ERROR_CLASS[error] || ERROR_UNKNOWN, aError);
    } else if (aError.message &&
        (aError.message === "INVALID_PARAMETER" ||
        aError.message === "NO_ACCOUNT" ||
        aError.message === "UNVERIFIED_ACCOUNT")) {
      return aError;
    }
    return this._error(ERROR_UNKNOWN, aError);
  },

  _error: function(aError, aDetails) {
    log.error("FxA rejecting with error ${aError}, details: ${aDetails}", {aError, aDetails});
    let reason = new Error(aError);
    if (aDetails) {
      reason.details = aDetails;
    }
    return reason;
  },

  /**
   * Get the user's account and profile data
   *
   * @param options
   *        {
   *          contentUrl: (string) Used by the FxAccountsWebChannel.
   *            Defaults to pref identity.fxaccounts.settings.uri
   *          profileServerUrl: (string) Used by the FxAccountsWebChannel.
   *            Defaults to pref identity.fxaccounts.remote.profile.uri
   *        }
   *
   * @return Promise.<object | Error>
   *        The promise resolves to an accountData object with extra profile
   *        information such as profileImageUrl, or rejects with
   *        an error object ({error: ERROR, details: {}}) of the following:
   *          INVALID_PARAMETER
   *          NO_ACCOUNT
   *          UNVERIFIED_ACCOUNT
   *          NETWORK_ERROR
   *          AUTH_ERROR
   *          UNKNOWN_ERROR
   */
  getSignedInUserProfile: function () {
    let currentState = this.currentAccountState;
    return this.profile.getProfile().then(
      profileData => {
        let profile = Cu.cloneInto(profileData, {});
        return currentState.resolve(profile);
      },
      error => {
        log.error("Could not retrieve profile data", error);
        return currentState.reject(error);
      }
    ).catch(err => Promise.reject(this._errorToErrorClass(err)));
  },

  registerDeviceIfNotRegistered() {
    return this.getSignedInUser().then(signedInUser => {
      if (signedInUser && !signedInUser.deviceId) {
        return this._registerOrUpdateDevice(signedInUser);
      }
    })
  },

  updateDevice() {
    return this.getSignedInUser().then(signedInUser => {
      if (signedInUser) {
        return this._registerOrUpdateDevice(signedInUser);
      }
    });
  },

  _registerOrUpdateDevice(signedInUser) {
    let deviceName = this._getDeviceName(), promise;

    if (signedInUser.deviceId) {
      log.debug("updating existing device details");
      promise = this.fxAccountsClient.deviceUpdate(
        signedInUser.sessionToken,
        signedInUser.deviceId,
        deviceName
      );
    } else {
      log.debug("registering new device details");
      promise = this.fxAccountsClient.deviceRegister(
        signedInUser.sessionToken,
        deviceName,
        "desktop"
      ).then(response => {
        return this.currentAccountState.updateUserAccountData({
          deviceId: response.id
        });
      });
    }

    return promise.catch(this._handleDeviceError.bind(this));
  },

  _getDeviceName() {
    try {
      return Services.prefs.getCharPref("services.sync.client.name");
    } catch (ignore) {
      // TODO: fix tests to stop this throwing
      return Utils.getDefaultDeviceName();
    }
  },

  _handleDeviceError(error) {
    if (error.code === 400) {
      if (error.errno === ERRNO_UNKNOWN_DEVICE) {
        log.warn("unknown device id, attempting to register new device instead");
        return this._forceUpdateDevice(null);
      }

      if (error.errno === ERRNO_DEVICE_SESSION_CONFLICT) {
        log.warn("device session conflict, attempting to untangle things");
        return this._recoverFromDeviceError(error, signedInUser.sessionToken);
      }
    }

    return this._failDeviceRegistration(error);
  },

  _forceUpdateDevice(deviceId) {
    return this.currentAccountState.updateUserAccountData({ deviceId })
      .then(this.updateDevice.bind(this));
  },

  _recoverFromDeviceError(error, sessionToken) {
    return this.fxAccountsClient.deviceList(sessionToken)
      .then(devices => {
        let matchingDevices = devices.filter(device => device.isCurrentDevice)
        if (matchingDevices.length === 1) {
          return this._forceUpdateDevice(matchingDevices[0].id);
        }
        return this._failDeviceRegistration(error);
      })
      .catch(ignore => this._failDeviceRegistration(error));
  },

  _failDeviceRegistration(error) {
    log.error("device registration failed", error);
    return Promise.reject(this._errorToErrorClass(error));
  }
};


// A getter for the instance to export
XPCOMUtils.defineLazyGetter(this, "fxAccounts", function() {
  let a = new FxAccounts();

  // XXX Bug 947061 - We need a strategy for resuming email verification after
  // browser restart
  a.loadAndPoll();

  return a;
});
