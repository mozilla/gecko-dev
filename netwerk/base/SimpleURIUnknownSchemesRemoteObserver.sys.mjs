/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
});

const SETTINGS_DEFAULTURI_BYPASS_LIST_KEY =
  "url-parser-default-unknown-schemes-interventions";

export class SimpleURIUnknownSchemesRemoteObserver {
  #initialized = false;
  #bypassListSettings;
  classID = Components.ID("{86606ba1-de17-4df4-9013-e571ab94fd94}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIObserver",
    "nsISimpleURIUnknownSchemesRemoteObserver",
  ]);

  observe(subject, topic) {
    // signal selected because RemoteSettingsClient is first getting initialised
    // by the AddonManager at addons-startup
    if (topic == "profile-after-change" && !this.#initialized) {
      this.#initialized = true;
      this.#init();
    }
  }

  /**
   * This method updates the io service with the local scheme list used to
   * bypass the defaultURI parser and use the simpleURI parser.
   * It also subscribes to Remote Settings changes to this list which are then
   * broadcast to processes interested in URL parsing.
   *
   * note that there doesn't appear to be a way to get a URI with a non-special
   * scheme into about:preferences so it should be safe to spin this up early
   */
  async #init() {
    if (!this.#bypassListSettings) {
      this.#bypassListSettings = lazy.RemoteSettings(
        SETTINGS_DEFAULTURI_BYPASS_LIST_KEY
      );
    }

    // Trigger a get from local remote settings and update the io service.
    const settingsList = await this.#getBypassList();
    let schemes = settingsList.map(r => r.scheme);
    if (schemes.length) {
      Services.io.setSimpleURIUnknownRemoteSchemes(schemes);
    }

    // Listen for future updates after we first get the values.
    this.#bypassListSettings.on("sync", this.#updateBypassList.bind(this));
  }

  async #updateBypassList() {
    const settingsList = await this.#getBypassList();
    let schemes = settingsList.map(r => r.scheme);
    if (schemes.length) {
      Services.io.setSimpleURIUnknownRemoteSchemes(schemes);
    }
  }

  async #getBypassList() {
    if (this._getSettingsPromise) {
      return this._getSettingsPromise;
    }

    const settings = await (this._getSettingsPromise =
      this.#getBypassListSettings());
    delete this._getSettingsPromise;
    return settings;
  }

  /**
   * Obtains the current bypass list from remote settings. This includes
   * verifying the signature of the bypass list within the database.
   *
   * If the signature in the database is invalid, the database will be wiped
   * and the stored dump will be used, until the settings next update.
   *
   * Note that this may cause a network check of the certificate, but that
   * should generally be quick.
   *
   * @param {boolean} [firstTime]
   *   Internal boolean to indicate if this is the first time check or not.
   * @returns {array}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async #getBypassListSettings(firstTime = true) {
    let result = [];
    try {
      result = await this.#bypassListSettings.get({
        verifySignature: true,
      });
    } catch (ex) {
      if (
        ex instanceof lazy.RemoteSettingsClient.InvalidSignatureError &&
        firstTime
      ) {
        // The local database is invalid, try and reset it.
        await this.#bypassListSettings.db.clear();
        // Now call this again.
        return this.#getBypassListSettings(false);
      }
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      console.error(ex);
    }
    return result;
  }
}
