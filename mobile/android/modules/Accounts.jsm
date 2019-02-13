/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Accounts"];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

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
let Accounts = Object.freeze({
  _accountsExist: function (kind) {
    return Messaging.sendRequestForResult({
      type: "Accounts:Exist",
      kind: kind
    }).then(data => data.exists);
  },

  firefoxAccountsExist: function () {
    return this._accountsExist("fxa");
  },

  syncAccountsExist: function () {
    return this._accountsExist("sync11");
  },

  anySyncAccountsExist: function () {
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
  launchSetup: function (extras) {
    Messaging.sendRequest({
      type: "Accounts:Create",
      extras: extras
    });
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
  createFirefoxAccountFromJSON: function (json) {
    return Messaging.sendRequestForResponse({
      type: "Accounts:CreateFirefoxAccountFromJSON",
      json: json
    });
  }
});
