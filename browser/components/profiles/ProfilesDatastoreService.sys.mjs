/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { DeferredTask } from "resource://gre/modules/DeferredTask.sys.mjs";

const NOTIFY_TIMEOUT = 200;
const STOREID_PREF_NAME = "toolkit.profiles.storeID";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

class ProfilesDatastoreServiceClass {
  #connection = null;
  #asyncShutdownBlocker = null;
  #initialized = false;
  #storeID = null;
  #initPromise = null;
  #notifyTask = null;
  #profileService = null;
  static #dirSvc = null;

  get storeID() {
    return new Promise(resolve => {
      this.init().then(() => {
        resolve(this.#storeID);
      });
    });
  }

  get initialized() {
    return this.#initialized;
  }

  static get PROFILE_GROUPS_DIR() {
    if (this.#dirSvc && "ProfileGroups" in this.#dirSvc) {
      return this.#dirSvc.ProfileGroups;
    }

    return PathUtils.join(
      ProfilesDatastoreServiceClass.getDirectory("UAppData").path,
      "Profile Groups"
    );
  }

  overrideDirectoryService(dirSvc) {
    if (!Cu.isInAutomation) {
      return;
    }

    ProfilesDatastoreServiceClass.#dirSvc = dirSvc;
  }

  static getDirectory(id) {
    if (this.#dirSvc) {
      if (id in this.#dirSvc) {
        return this.#dirSvc[id].clone();
      }
    }

    return Services.dirsvc.get(id, Ci.nsIFile);
  }

  /**
   * For use in testing only, override the profile service with a mock version
   * and reset state accordingly.
   *
   * @param {Ci.nsIToolkitProfileService} profileService The mock profile service
   */
  async resetProfileService(profileService) {
    if (!Cu.isInAutomation) {
      return;
    }

    await this.uninit();
    this.#profileService =
      profileService ??
      Cc["@mozilla.org/toolkit/profile-service;1"].getService(
        Ci.nsIToolkitProfileService
      );
    await this.init();
  }

  async getConnection() {
    await this.init();
    return this.#connection;
  }

  get toolkitProfileService() {
    return this.#profileService;
  }

  constructor() {
    this.#asyncShutdownBlocker = () => this.uninit();
    this.#profileService = Cc[
      "@mozilla.org/toolkit/profile-service;1"
    ].getService(Ci.nsIToolkitProfileService);
  }

  /**
   * At startup, get the groupDBPath from the prefs, and connect to it.
   *
   * @returns {Promise}
   */
  init() {
    if (!this.#initPromise) {
      this.#initPromise = this.#init().finally(
        () => (this.#initPromise = null)
      );
    }

    return this.#initPromise;
  }

  async #init() {
    if (this.#initialized) {
      return;
    }

    this.#storeID = Services.prefs.getStringPref(STOREID_PREF_NAME, "");

    // This could fail if we're adding it during shutdown. In this case,
    // don't throw but don't continue initialization.
    try {
      lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
        "ProfilesDatastoreService uninit",
        this.#asyncShutdownBlocker
      );
    } catch (ex) {
      console.error(ex);
      return;
    }

    this.#notifyTask = new DeferredTask(async () => {
      this.#datastoreChanged("local");
    }, NOTIFY_TIMEOUT);

    try {
      await this.#initConnection();
    } catch (e) {
      console.error(e);

      await this.uninit();
      return;
    }

    this.#initialized = true;
  }

  async uninit() {
    lazy.AsyncShutdown.profileChangeTeardown.removeBlocker(
      this.#asyncShutdownBlocker
    );

    // During shutdown we don't need to notify ourselves, just other instances
    // so rather than finalizing the task just disarm it and do the notification
    // manually.
    if (this.#notifyTask.isArmed) {
      this.#notifyTask.disarm();
      this.#datastoreChanged("shutdown");
    }

    await this.closeConnection();

    this.#storeID = null;

    this.#initialized = false;
  }

  async #initConnection() {
    if (this.#connection) {
      return;
    }

    let path = await this.getProfilesStorePath();

    // TODO: (Bug 1902320) Handle exceptions on connection opening
    // This could fail if the store is corrupted.
    this.#connection = await lazy.Sqlite.openConnection({
      path,
      openNotExclusive: true,
    });

    await this.#connection.execute("PRAGMA journal_mode = WAL");
    await this.#connection.execute("PRAGMA wal_autocheckpoint = 16");

    await this.createTables();
  }

  async closeConnection() {
    if (!this.#connection) {
      return;
    }

    // An error could occur while closing the connection. We suppress the
    // error since it is not a critical part of the browser.
    try {
      await this.#connection.close();
    } catch (ex) {}
    this.#connection = null;
  }

  /**
   * Create tables for Selectable Profiles if they don't already exist
   */
  async createTables() {
    // TODO: (Bug 1902320) Handle exceptions on connection opening
    await this.#connection.executeTransaction(async () => {
      const createProfilesTable = `
          CREATE TABLE IF NOT EXISTS "Profiles" (
            id  INTEGER NOT NULL,
            path	TEXT NOT NULL UNIQUE,
            name	TEXT NOT NULL,
            avatar	TEXT NOT NULL,
            themeId	TEXT NOT NULL,
            themeFg	TEXT NOT NULL,
            themeBg	TEXT NOT NULL,
            PRIMARY KEY(id)
          );`;

      await this.#connection.execute(createProfilesTable);

      const createSharedPrefsTable = `
          CREATE TABLE IF NOT EXISTS "SharedPrefs" (
            id	INTEGER NOT NULL,
            name	TEXT NOT NULL UNIQUE,
            value	BLOB,
            isBoolean	INTEGER,
            PRIMARY KEY(id)
          );`;

      await this.#connection.execute(createSharedPrefsTable);
    });
  }

  async maybeCreateProfilesStorePath() {
    if (this.#storeID) {
      return;
    }

    await IOUtils.makeDirectory(
      ProfilesDatastoreServiceClass.PROFILE_GROUPS_DIR
    );

    const storageID = Services.uuid
      .generateUUID()
      .toString()
      .replace("{", "")
      .split("-")[0];

    this.#storeID = storageID;
    Services.prefs.setStringPref(STOREID_PREF_NAME, storageID);
  }

  async getProfilesStorePath() {
    await this.maybeCreateProfilesStorePath();

    // If we are not running in a named nsIToolkitProfile, the datastore path
    // should be in the profile directory. This is true in a local build or a
    // CI test build, for example.
    if (
      !this.#profileService.currentProfile &&
      !this.#profileService.groupProfile
    ) {
      return PathUtils.join(
        ProfilesDatastoreServiceClass.getDirectory("ProfD").path,
        `${this.#storeID}.sqlite`
      );
    }

    return PathUtils.join(
      ProfilesDatastoreServiceClass.PROFILE_GROUPS_DIR,
      `${this.#storeID}.sqlite`
    );
  }

  notify() {
    this.#notifyTask.arm();
  }

  /**
   * Notify datastore observers that the data has changed by firing
   * the "pds-datastore-changed" nsIObserver signal.
   *
   *@param {"local"|"shutdown"} source The source of the
   *   notification. Either "local" meaning that the change was made in this
   *   process and "shutdown" meaning we are closing the connection and
   *   shutting down.
   */
  #datastoreChanged(source) {
    Services.obs.notifyObservers(null, "pds-datastore-changed", source);
  }
}

const ProfilesDatastoreService = new ProfilesDatastoreServiceClass();
export { ProfilesDatastoreService };
