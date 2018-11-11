/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Accounts"];

ChromeUtils.import("resource://gre/modules/Messaging.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * A promise-based API for querying the existence of Sync accounts,
 * and accessing the Sync setup wizard.
 *
 * Usage:
 *
 *   Cu.import("resource://gre/modules/Accounts.jsm");
 *   Accounts.anySyncAccountsExist().then(
 *     (exist) => {
 *       console.log("Accounts exist? " + exist);
 *       if (!exist) {
 *         Accounts.launchSetup();
 *       }
 *     },
 *     (err) => {
 *       console.log("We failed so hard.");
 *     }
 *   );
 */
var Accounts = Object.freeze({
  _accountsExist: function(kind) {
    return EventDispatcher.instance.sendRequestForResult({
      type: "Accounts:Exist",
      kind: kind,
    }).then(data => data.exists);
  },

  firefoxAccountsExist: function() {
    return this._accountsExist("fxa");
  },

  anySyncAccountsExist: function() {
    return this._accountsExist("any");
  },

  /**
   * Fire-and-forget: open the Firefox accounts activity, which
   * will be the Getting Started screen if FxA isn't yet set up.
   *
   * Optional extras are passed, as a JSON string, to the Firefox
   * Account Getting Started activity in the extras bundle of the
   * activity launch intent, under the key "extras".
   *
   * There is no return value from this method.
   */
  launchSetup: function(extras) {
    EventDispatcher.instance.sendRequest({
      type: "Accounts:Create",
      extras: extras,
    });
  },

  _addDefaultEndpoints: function(json) {
    let newData = Cu.cloneInto(json, {}, { cloneFunctions: false });
    let associations = {
      authServerEndpoint: "identity.fxaccounts.auth.uri",
      profileServerEndpoint: "identity.fxaccounts.remote.profile.uri",
      tokenServerEndpoint: "identity.sync.tokenserver.uri",
    };
    for (let key in associations) {
      newData[key] = newData[key] || Services.urlFormatter.formatURLPref(associations[key]);
    }
    return newData;
  },

  /**
   * Create a new Android Account corresponding to the given
   * fxa-content-server "login" JSON datum.  The new account will be
   * in the "Engaged" state, and will start syncing immediately.
   *
   * It is an error if an Android Account already exists.
   *
   * Returns a Promise that resolves to a boolean indicating success.
   */
  createFirefoxAccountFromJSON: function(json) {
    return EventDispatcher.instance.sendRequestForResult({
      type: "Accounts:CreateFirefoxAccountFromJSON",
      json: this._addDefaultEndpoints(json),
    });
  },

  /**
   * Move an existing Android Account to the "Engaged" state with the given
   * fxa-content-server "login" JSON datum.  The account will (re)start
   * syncing immediately, unless the user has manually configured the account
   * to not Sync.
   *
   * It is an error if no Android Account exists.
   *
   * Returns a Promise that resolves to a boolean indicating success.
   */
  updateFirefoxAccountFromJSON: function(json) {
    return EventDispatcher.instance.sendRequestForResult({
      type: "Accounts:UpdateFirefoxAccountFromJSON",
      json: this._addDefaultEndpoints(json),
    });
  },

  /**
   * Notify that profile for Android Account has updated.
   * The account will re-fetch the profile image.
   *
   * It is an error if no Android Account exists.
   *
   * There is no return value from this method.
   */
  notifyFirefoxAccountProfileChanged: function() {
    EventDispatcher.instance.sendRequest({
      type: "Accounts:ProfileUpdated",
    });
  },

  /**
   * Fetch information about an existing Android Firefox Account.
   *
   * Returns a Promise that resolves to null if no Android Firefox Account
   * exists, or an object including at least a string-valued 'email' key.
   */
  getFirefoxAccount: function() {
    return EventDispatcher.instance.sendRequestForResult({
      type: "Accounts:Exist",
      kind: "fxa",
    }).then(data => {
      if (!data || !data.exists) {
        return null;
      }
      delete data.exists;
      return data;
    });
  },

  /**
   * Delete an existing Android Firefox Account.
   *
   * It is an error if no Android Account exists.
   *
   * Returns a Promise that resolves to a boolean indicating success.
   */
  deleteFirefoxAccount: function() {
    return EventDispatcher.instance.sendRequestForResult({
      type: "Accounts:DeleteFirefoxAccount",
    });
  },

  showSyncPreferences: function() {
    // Only show Sync preferences of an existing Android Account.
    return Accounts.getFirefoxAccount().then(account => {
      if (!account) {
        throw new Error("Can't show Sync preferences of non-existent Firefox Account!");
      }
      return EventDispatcher.instance.sendRequestForResult({
        type: "Accounts:ShowSyncPreferences",
      });
    });
  },
});
