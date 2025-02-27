/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
});

export const ESSENTIAL_DOMAINS_REMOTE_BUCKET = "moz-essential-domain-fallbacks";

export class EssentialDomainsRemoteSettings {
  #initialized = false;
  #fallbackDomains;
  classID = Components.ID("{962dbf40-2c3f-4c1f-8ae8-90e8c9d85368}");
  QueryInterface = ChromeUtils.generateQI(["nsIObserver"]);

  observe(subject, topic) {
    // signal selected because RemoteSettingsClient is first getting initialised
    // by the AddonManager at addons-startup
    if (topic == "profile-after-change" || !this.#initialized) {
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
    if (!this.#fallbackDomains) {
      this.#fallbackDomains = lazy.RemoteSettings(
        ESSENTIAL_DOMAINS_REMOTE_BUCKET
      );
    }

    // Trigger a get from local remote settings and update the io service.
    const settingsList = await this.#getFallbackList();
    for (let setting of settingsList) {
      Services.io.addEssentialDomainMapping(setting.from, setting.to);
    }

    // Listen for future updates after we first get the values.
    this.#fallbackDomains.on("sync", this.#updateFallbackDomains.bind(this));
  }

  async #updateFallbackDomains() {
    Services.io.clearEssentialDomainMapping();

    const settingsList = await this.#getFallbackList();
    for (let setting of settingsList) {
      Services.io.addEssentialDomainMapping(setting.from, setting.to);
    }
  }

  async #getFallbackList() {
    if (this._getSettingsPromise) {
      return this._getSettingsPromise;
    }

    const settings = await (this._getSettingsPromise =
      this.#getFallbackDomains());
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
  async #getFallbackDomains(firstTime = true) {
    let result = [];
    try {
      result = await this.#fallbackDomains.get({
        verifySignature: true,
      });
    } catch (ex) {
      if (
        ex instanceof lazy.RemoteSettingsClient.InvalidSignatureError &&
        firstTime
      ) {
        // The local database is invalid, try and reset it.
        await this.#fallbackDomains.db.clear();
        // Now call this again.
        return this.#getFallbackDomains(false);
      }
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      console.error(ex);
    }
    return result;
  }
}
