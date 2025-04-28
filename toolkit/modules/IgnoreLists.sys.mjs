/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
});

/**
 * @typedef {import("../../services/settings/RemoteSettingsClient.sys.mjs").RemoteSettingsClient} RemoteSettingsClient
 */

const SETTINGS_IGNORELIST_KEY = "hijack-blocklists";

/**
 * A remote settings wrapper for the ignore lists from the hijack-blocklists
 * collection.
 */
class IgnoreListsManager {
  /**
   * @type {RemoteSettingsClient}
   */
  #ignoreListSettings;

  /**
   * Initializes the manager, if it is not already initialised.
   */
  #init() {
    if (!this.#ignoreListSettings) {
      this.#ignoreListSettings = lazy.RemoteSettings(SETTINGS_IGNORELIST_KEY);
    }
  }

  /**
   * Gets the current collection, subscribing to the collection after the
   * get has been completed.
   *
   * @param {Function} listener
   */
  async getAndSubscribe(listener) {
    this.#init();

    // Trigger a get of the initial value.
    const settings = await this.#getIgnoreList();

    // Listen for future updates after we first get the values.
    this.#ignoreListSettings.on("sync", listener);

    return settings;
  }

  /**
   * Unsubscribes from updates to the collection.
   *
   * @param {Function} listener
   */
  unsubscribe(listener) {
    if (!this.#ignoreListSettings) {
      return;
    }

    this.#ignoreListSettings.off("sync", listener);
  }

  /**
   * @type {Promise<object[]>}
   */
  #getSettingsPromise;

  async #getIgnoreList() {
    if (this.#getSettingsPromise) {
      return this.#getSettingsPromise;
    }

    const settings = await (this.#getSettingsPromise =
      this.#getIgnoreListSettings());
    this.#getSettingsPromise = undefined;
    return settings;
  }

  /**
   * Obtains the current ignore list from remote settings. This includes
   * verifying the signature of the ignore list within the database.
   *
   * If the signature in the database is invalid, the database will be wiped
   * and the stored dump will be used, until the settings next update.
   *
   * Note that this may cause a network check of the certificate, but that
   * should generally be quick.
   *
   * @param {boolean} [firstTime]
   *   Internal boolean to indicate if this is the first time check or not.
   * @returns {Promise<object[]>}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async #getIgnoreListSettings(firstTime = true) {
    let result = [];
    try {
      result = await this.#ignoreListSettings.get({
        verifySignature: true,
      });
    } catch (ex) {
      if (
        ex instanceof lazy.RemoteSettingsClient.InvalidSignatureError &&
        firstTime
      ) {
        // The local database is invalid, try and reset it.
        await this.#ignoreListSettings.db.clear();
        // Now call this again.
        return this.#getIgnoreListSettings(false);
      }
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      console.error(ex);
    }
    return result;
  }
}

/**
 * A remote settings wrapper for the ignore lists from the hijack-blocklists
 * collection.
 */
export const IgnoreLists = new IgnoreListsManager();
