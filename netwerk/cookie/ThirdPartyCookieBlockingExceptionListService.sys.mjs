/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

// Name of the RemoteSettings collection containing the records.
const COLLECTION_NAME = "third-party-cookie-blocking-exempt-urls";
const PREF_NAME = "network.cookie.cookieBehavior.optInPartitioning.skip_list";

export class ThirdPartyCookieBlockingExceptionListService {
  classId = Components.ID("{1ee0cc18-c968-4105-a895-bdea08e187eb}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIThirdPartyCookieBlockingExceptionListService",
  ]);

  #rs = null;
  #onSyncCallback = null;

  // Sets to keep track of the exceptions in the pref. It uses the string in the
  // format "firstPartySite,thirdPartySite" as the key.
  #prefValueSet = null;
  // Set to keep track of exceptions from RemoteSettings. It uses the same
  // keying as above.
  #rsValueSet = null;

  constructor() {
    this.#rs = lazy.RemoteSettings(COLLECTION_NAME);
  }

  async init() {
    await this.importAllExceptions();

    Services.prefs.addObserver(PREF_NAME, this);

    if (!this.#onSyncCallback) {
      this.#onSyncCallback = this.onSync.bind(this);
      this.#rs.on("sync", this.#onSyncCallback);
    }

    // Import for initial pref state.
    this.onPrefChange();
  }

  shutdown() {
    Services.prefs.removeObserver(PREF_NAME, this);

    if (this.#onSyncCallback) {
      this.#rs.off("sync", this.#onSyncCallback);
      this.#onSyncCallback = null;
    }
  }

  #handleExceptionChange(created = [], deleted = []) {
    if (created.length) {
      // TODO: Calling CookieService API to remove exception sites.
    }
    if (deleted.length) {
      // TODO: Calling CookieService API to add exception sites.
    }
  }

  onSync({ data: { created = [], updated = [], deleted = [] } }) {
    // Convert the RemoteSettings records to exception entries.
    created = created.map(ex =>
      ThirdPartyCookieExceptionEntry.fromRemoteSettingsRecord(ex)
    );
    deleted = deleted.map(ex =>
      ThirdPartyCookieExceptionEntry.fromRemoteSettingsRecord(ex)
    );

    updated.forEach(ex => {
      let newEntry = ThirdPartyCookieExceptionEntry.fromRemoteSettingsRecord(
        ex.new
      );
      let oldEntry = ThirdPartyCookieExceptionEntry.fromRemoteSettingsRecord(
        ex.old
      );

      // We only care about changes in the sites.
      if (newEntry.equals(oldEntry)) {
        return;
      }
      created.push(newEntry);
      deleted.push(oldEntry);
    });

    this.#rsValueSet ??= new Set();

    // Remove items in sitesToRemove
    for (const site of deleted) {
      this.#rsValueSet.delete(site.serialize());
    }

    // Add items from sitesToAdd
    for (const site of created) {
      this.#rsValueSet.add(site.serialize());
    }

    this.#handleExceptionChange(created, deleted);
  }

  onPrefChange() {
    let newExceptions = Services.prefs.getStringPref(PREF_NAME, "").split(";");

    // Convert the exception strings to exception entries.
    newExceptions = newExceptions
      .map(ex => ThirdPartyCookieExceptionEntry.fromString(ex))
      .filter(Boolean);

    // If this is the first time we're initializing from pref, we can directly
    // call handleExceptionChange to create the exceptions.
    if (!this.#prefValueSet) {
      this.#handleExceptionChange({
        data: { created: newExceptions },
        prefUpdate: true,
      });
      // Serialize the exception entries to the string format and store in the
      // pref set.
      this.#prefValueSet = new Set(newExceptions.map(ex => ex.serialize()));
      return;
    }

    // Otherwise, we need to check for changes in the pref.

    // Find added items
    let created = [...newExceptions].filter(
      ex => !this.#prefValueSet.has(ex.serialize())
    );

    // Convert the new exceptions to the string format to check against the pref
    // set.
    let newExceptionStringSet = new Set(
      newExceptions.map(ex => ex.serialize())
    );

    // Find removed items
    let deleted = Array.from(this.#prefValueSet)
      .filter(item => !newExceptionStringSet.has(item))
      .map(ex => ThirdPartyCookieExceptionEntry.fromString(ex));

    // We shouldn't remove the exceptions in the remote settings list.
    if (this.#rsValueSet) {
      deleted = deleted.filter(ex => !this.#rsValueSet.has(ex.serialize()));
    }

    this.#prefValueSet = newExceptionStringSet;

    // Calling handleExceptionChange to handle the changes.
    this.#handleExceptionChange(created, deleted);
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed" || data != PREF_NAME) {
      throw new Error(`Unexpected event ${topic} with ${data}`);
    }

    this.onPrefChange();
  }

  async importAllExceptions() {
    try {
      let exceptions = await this.#rs.get();
      if (!exceptions.length) {
        return;
      }
      this.onSync({ data: { created: exceptions } });
    } catch (error) {
      console.error(
        "Error while importing 3pcb exceptions from RemoteSettings",
        error
      );
    }
  }
}

export class ThirdPartyCookieExceptionEntry {
  classId = Components.ID("{8200e12c-416c-42eb-8af5-db9745d2e527}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIThirdPartyCookieExceptionEntry",
  ]);

  constructor(fpSite, tpSite) {
    this.firstPartySite = fpSite;
    this.thirdPartySite = tpSite;
  }

  // Serialize the exception entry into a string. This is used for keying the
  // exception in the pref and RemoteSettings set.
  serialize() {
    return `${this.firstPartySite},${this.thirdPartySite}`;
  }

  equals(other) {
    return (
      this.firstPartySite === other.firstPartySite &&
      this.thirdPartySite === other.thirdPartySite
    );
  }

  static fromString(exStr) {
    if (!exStr) {
      return null;
    }

    let [fpSite, tpSite] = exStr.split(",");
    try {
      fpSite = this.#sanitizeSite(fpSite, true);
      tpSite = this.#sanitizeSite(tpSite);

      return new ThirdPartyCookieExceptionEntry(fpSite, tpSite);
    } catch (e) {
      console.error(
        `Error while constructing 3pcd exception entry from string`,
        exStr
      );
      return null;
    }
  }

  static fromRemoteSettingsRecord(record) {
    try {
      let fpSite = this.#sanitizeSite(record.fpSite, true);
      let tpSite = this.#sanitizeSite(record.tpSite);

      return new ThirdPartyCookieExceptionEntry(fpSite, tpSite);
    } catch (e) {
      console.error(
        `Error while constructing 3pcd exception entry from RemoteSettings record`,
        record
      );
      return null;
    }
  }

  // A helper function to sanitize the site using the eTLD service.
  static #sanitizeSite(site, acceptWildcard = false) {
    if (acceptWildcard && site === "*") {
      return "*";
    }

    let uri = Services.io.newURI(site);
    return Services.eTLD.getSite(uri);
  }
}
