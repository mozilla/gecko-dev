/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TDOD: Remove eslint-disable lines once methods are updated. See bug 1896727
/* eslint-disable no-unused-vars */
/* eslint-disable no-unused-private-class-members */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { SelectableProfile } from "./SelectableProfile.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CryptoUtils: "resource://services-crypto/utils.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

const PROFILES_CRYPTO_SALT_LENGTH_BYTES = 16;

/**
 * The service that manages selectable profiles
 */
class SelectableProfileServiceClass {
  #connection = null;
  #asyncShutdownBlocker = null;
  #initialized = false;
  #groupToolkitProfile = null;
  #currentProfile = null;
  #everyWindowCallbackId = "SelectableProfileService";

  // Do not use. Only for testing.
  constructor() {
    if (Cu.isInAutomation) {
      this.#groupToolkitProfile = {
        storeID: "12345678",
        rootDir: Services.dirsvc.get("ProfD", Ci.nsIFile),
      };
    } else {
      this.#groupToolkitProfile =
        lazy.ProfileService.currentProfile ?? lazy.ProfileService.groupProfile;
    }
  }

  get groupToolkitProfile() {
    return this.#groupToolkitProfile;
  }

  get toolkitProfileRootDir() {
    return this.#groupToolkitProfile.rootDir;
  }

  get currentProfile() {
    return this.#currentProfile;
  }

  get initialized() {
    return this.#initialized;
  }

  static get PROFILE_GROUPS_DIR() {
    return PathUtils.join(
      Services.dirsvc.get("UAppData", Ci.nsIFile).path,
      "Profile Groups"
    );
  }

  async createProfilesStorePath() {
    await IOUtils.makeDirectory(
      SelectableProfileServiceClass.PROFILE_GROUPS_DIR
    );

    const storageID = Services.uuid
      .generateUUID()
      .toString()
      .replace("{", "")
      .split("-")[0];
    this.#groupToolkitProfile.storeID = storageID;
    lazy.ProfileService.flush();
  }

  async getProfilesStorePath() {
    if (!this.#groupToolkitProfile.storeID) {
      await this.createProfilesStorePath();
    }

    return PathUtils.join(
      SelectableProfileServiceClass.PROFILE_GROUPS_DIR,
      `${this.#groupToolkitProfile.storeID}.sqlite`
    );
  }

  /**
   * At startup, store the nsToolkitProfile for the group.
   * Get the groupDBPath from the nsToolkitProfile, and connect to it.
   */
  async init() {
    if (this.#initialized) {
      return;
    }

    await this.initConnection();

    // When we launch into the startup window, the `ProfD` is not defined so
    // Services.dirsvc.get("ProfD", Ci.nsIFile) will throw. Leaving the
    // `currentProfile` as null is fine for the startup window.
    try {
      // Get the SelectableProfile by the profile directory
      this.#currentProfile = await this.getProfileByPath(
        Services.dirsvc.get("ProfD", Ci.nsIFile)
      );
    } catch {}

    // The 'activate' event listeners use #currentProfile, so this line has
    // to come after #currentProfile has been set.
    this.initWindowTracker();

    this.#initialized = true;
  }

  async uninit() {
    if (!this.#initialized) {
      return;
    }

    lazy.EveryWindow.unregisterCallback(this.#everyWindowCallbackId);

    await this.closeConnection();

    this.#currentProfile = null;

    this.#initialized = false;
  }

  initWindowTracker() {
    lazy.EveryWindow.registerCallback(
      this.#everyWindowCallbackId,
      window => {
        let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(window);
        if (isPBM) {
          return;
        }

        window.addEventListener("activate", this);
      },
      window => {
        let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(window);
        if (isPBM) {
          return;
        }

        window.removeEventListener("activate", this);
      }
    );
  }

  async initConnection() {
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

    this.#asyncShutdownBlocker = async () => {
      await this.#connection.close();
      this.#connection = null;
    };

    // This could fail if we're adding it during shutdown. In this case,
    // don't throw but close the connection.
    try {
      lazy.Sqlite.shutdown.addBlocker(
        "Profiles:ProfilesSqlite closing",
        this.#asyncShutdownBlocker
      );
    } catch (ex) {
      await this.closeConnection();
      return;
    }

    await this.createProfilesDBTables();
  }

  async closeConnection() {
    if (this.#asyncShutdownBlocker) {
      lazy.Sqlite.shutdown.removeBlocker(this.#asyncShutdownBlocker);
      this.#asyncShutdownBlocker = null;
    }

    if (this.#connection) {
      // An error could occur while closing the connection. We suppress the
      // error since it is not a critical part of the browser.
      try {
        await this.#connection.close();
      } catch (ex) {}
      this.#connection = null;
    }
  }

  async handleEvent(event) {
    switch (event.type) {
      case "activate": {
        if (
          this.#groupToolkitProfile.rootDir.path === this.currentProfile.path
        ) {
          return;
        }
        this.#groupToolkitProfile.rootDir = await this.currentProfile.rootDir;
        lazy.ProfileService.flush();
      }
    }
  }

  /**
   * Create tables for Selectable Profiles if they don't already exist
   */
  async createProfilesDBTables() {
    // TODO: (Bug 1902320) Handle exceptions on connection opening
    await this.#connection.executeTransaction(async () => {
      const createProfilesTable = `
        CREATE TABLE IF NOT EXISTS "Profiles" (
          id  INTEGER NOT NULL,
          path	TEXT NOT NULL UNIQUE,
          name	TEXT NOT NULL,
          avatar	TEXT NOT NULL,
          themeL10nId	TEXT NOT NULL,
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

  /**
   * Create the SQLite DB for the profile group.
   * Init shared prefs for the group and add to DB.
   * Create the Group DB path to aNamedProfile entry in profiles.ini.
   * Import aNamedProfile into DB.
   */
  createProfileGroup() {}

  /**
   * When the last selectable profile in a group is deleted,
   * also remove the profile group's named profile entry from profiles.ini
   * and vacuum the group DB.
   */
  async deleteProfileGroup() {
    if (this.getAllProfiles().length) {
      return;
    }

    this.#groupToolkitProfile.storeID = null;
    lazy.ProfileService.flush();
    await this.vacuumAndCloseGroupDB();
  }

  // App session lifecycle methods and multi-process support

  /*
   * Helper that returns an inited Firefox executable process (nsIProcess).
   * Mostly useful for mocking in unit testing.
   */
  getExecutableProcess() {
    let process = Cc["@mozilla.org/process/util;1"].createInstance(
      Ci.nsIProcess
    );
    let executable = Services.dirsvc.get("XREExeF", Ci.nsIFile);
    process.init(executable);
    return process;
  }

  /**
   * Launch a new Firefox instance using the given selectable profile.
   *
   * @param {SelectableProfile} aProfile The profile to launch
   */
  launchInstance(aProfile) {
    let process = this.getExecutableProcess();
    let args = ["--profile", aProfile.path];
    if (Services.appinfo.OS === "Darwin") {
      args.unshift("-foreground");
    }
    process.runw(false, args, args.length);
  }

  /**
   * When the group DB has been updated, either changes to prefs or profiles,
   * ask the remoting service to notify other running instances that they should
   * check for updates and refresh their UI accordingly.
   */
  notify() {}

  /**
   * Invoked when the remoting service has notified this instance that another
   * instance has updated the database. Triggers refreshProfiles() and refreshPrefs().
   */
  observe() {}

  /**
   * Init or update the current SelectableProfiles from the DB.
   */
  refreshProfiles() {}

  /**
   * Fetch all prefs from the DB and write to the current instance.
   */
  refreshPrefs() {}

  /**
   * Update the current default profile by setting its path as the Path
   * of the nsToolkitProfile for the group.
   *
   * @param {SelectableProfile} aSelectableProfile The new default SelectableProfile
   */
  setDefault(aSelectableProfile) {}

  /**
   * Update whether to show the selectable profile selector window at startup.
   * Set on the nsToolkitProfile instance for the group.
   *
   * @param {boolean} shouldShow Whether or not we should show the profile selector
   */
  showProfileSelectorWindow(shouldShow) {
    if (shouldShow === this.groupToolkitProfile.showProfileSelector) {
      return;
    }

    this.groupToolkitProfile.showProfileSelector = shouldShow;
    lazy.ProfileService.flush();
  }

  /**
   * Update the path to the group DB. Set on the nsToolkitProfile instance
   * for the group.
   *
   * @param {string} aPath The path to the group DB
   */
  setGroupDBPath(aPath) {}

  // SelectableProfile lifecycle

  /**
   * Create the profile directory for new profile. The profile name is combined
   * with a salt string to ensure the directory is unique. The format of the
   * directory is salt + "." + profileName. (Ex. c7IZaLu7.testProfile)
   *
   * @param {string} aProfileName The name of the profile to be created
   * @returns {string} The relative path for the given profile
   */
  async createProfileDirs(aProfileName) {
    const salt = btoa(
      lazy.CryptoUtils.generateRandomBytesLegacy(
        PROFILES_CRYPTO_SALT_LENGTH_BYTES
      )
    );
    // Sometimes the string from CryptoUtils.generateRandomBytesLegacy will
    // contain non-word characters that we don't want to include in the profile
    // directory name. So we match only word characters for the directory name.
    const safeSalt = salt.match(/\w/g).join("").slice(0, 8);

    const profileDir = `${safeSalt}.${aProfileName}`;

    // Handle errors in bug 1909919
    await Promise.all([
      IOUtils.makeDirectory(
        PathUtils.join(
          Services.dirsvc.get("DefProfRt", Ci.nsIFile).path,
          profileDir
        )
      ),
      IOUtils.makeDirectory(
        PathUtils.join(
          Services.dirsvc.get("DefProfLRt", Ci.nsIFile).path,
          profileDir
        )
      ),
    ]);

    return IOUtils.getDirectory(
      PathUtils.join(
        Services.dirsvc.get("DefProfRt", Ci.nsIFile).path,
        profileDir
      )
    );
  }

  /**
   * Get a relative to the Profiles directory for the given profile directory.
   *
   * @param {nsIFile} aProfilePath Path to profile directory.
   *
   * @returns {string} A relative path of the profile directory.
   */
  getRelativeProfilePath(aProfilePath) {
    let relativePath = aProfilePath.getRelativePath(
      Services.dirsvc.get("UAppData", Ci.nsIFile)
    );

    if (AppConstants.platform === "win") {
      relativePath = relativePath.replace("/", "\\");
    }

    return relativePath;
  }

  /**
   * Create an empty SelectableProfile and add it to the group DB.
   * This is an unmanaged profile from the nsToolkitProfile perspective.
   *
   * @param {object} profile An object that contains a path, name, themeL10nId,
   *                 themeFg, and themeBg for creating a new profile.
   * @returns {SelectableProfile}
   *   The newly created profile object.
   */
  async createProfile(profile) {
    let profilePath = await this.createProfileDirs(profile.name);
    let relativePath = this.getRelativeProfilePath(profilePath);
    profile.path = relativePath;
    await this.#connection.execute(
      `INSERT INTO Profiles VALUES (NULL, :path, :name, :avatar, :themeL10nId, :themeFg, :themeBg);`,
      profile
    );
    return this.getProfileByPath(profilePath);
  }

  /**
   * Remove the profile directories.
   *
   * @param {SelectableProfile} aSelectableProfile The SelectableProfile of the
   * directories to be removed.
   */
  async removeProfileDirs(aSelectableProfile) {
    let profileDir = (await aSelectableProfile.rootDir).leafName;
    // Handle errors in bug 1909919
    await Promise.all([
      IOUtils.remove(
        PathUtils.join(
          Services.dirsvc.get("DefProfRt", Ci.nsIFile).path,
          profileDir
        ),
        {
          recursive: true,
        }
      ),
      IOUtils.remove(
        PathUtils.join(
          Services.dirsvc.get("DefProfLRt", Ci.nsIFile).path,
          profileDir
        ),
        {
          recursive: true,
        }
      ),
    ]);
  }

  /**
   * Delete a SelectableProfile from the group DB.
   * If it was the last profile in the group, also call deleteProfileGroup().
   *
   * @param {SelectableProfile} aSelectableProfile The SelectableProfile to be deleted
   * @param {boolean} removeFiles True if the profile directory should be removed
   */
  async deleteProfile(aSelectableProfile, removeFiles) {
    await this.#connection.execute("DELETE FROM Profiles WHERE id = :id;", {
      id: aSelectableProfile.id,
    });
    if (removeFiles) {
      await this.removeProfileDirs(aSelectableProfile);
    }
  }

  /**
   * Close all active instances running the current profile
   */
  closeActiveProfileInstances() {}

  /**
   * Schedule deletion of the current SelectableProfile as a background task, then exit.
   */
  async deleteCurrentProfile() {
    this.closeActiveProfileInstances();

    await this.#connection.executeBeforeShutdown(
      "SelectableProfileService: deleteCurrentProfile",
      db =>
        db.execute("DELETE FROM Profiles WHERE id = :id;", {
          id: this.currentProfile.id,
        })
    );
  }

  /**
   * Write an updated profile to the DB.
   *
   * @param {SelectableProfile} aSelectableProfile The SelectableProfile to be updated
   */
  async updateProfile(aSelectableProfile) {
    let profile = {
      id: aSelectableProfile.id,
      path: aSelectableProfile.path,
      name: aSelectableProfile.name,
      avatar: aSelectableProfile.avatar,
      ...aSelectableProfile.theme,
    };

    await this.#connection.execute(
      `UPDATE Profiles
       SET path = :path, name = :name, avatar = :avatar, themeL10nId = :themeL10nId, themeFg = :themeFg, themeBg = :themeBg
       WHERE id = :id;`,
      profile
    );
  }

  /**
   * Get the complete list of profiles in the group.
   *
   * @returns {Array<SelectableProfile>}
   *   An array of profiles in the group.
   */
  async getAllProfiles() {
    return (
      await this.#connection.executeCached("SELECT * FROM Profiles;")
    ).map(row => {
      return new SelectableProfile(row);
    });
  }

  /**
   * Get a specific profile by its internal ID.
   *
   * @param {number} aProfileID The internal id of the profile
   * @returns {SelectableProfile}
   *   The specific profile.
   */
  async getProfile(aProfileID) {
    let row = (
      await this.#connection.execute("SELECT * FROM Profiles WHERE id = :id;", {
        id: aProfileID,
      })
    )[0];

    return row ? new SelectableProfile(row) : null;
  }

  /**
   * Get a specific profile by its name.
   *
   * @param {string} aProfileNanme The name of the profile
   * @returns {SelectableProfile}
   *   The specific profile.
   */
  async getProfileByName(aProfileNanme) {
    let row = (
      await this.#connection.execute(
        "SELECT * FROM Profiles WHERE name = :name;",
        {
          name: aProfileNanme,
        }
      )
    )[0];

    return row ? new SelectableProfile(row) : null;
  }

  /**
   * Get a specific profile by its absolute path.
   *
   * @param {nsIFile} aProfilePath The path of the profile
   * @returns {SelectableProfile|null}
   */
  async getProfileByPath(aProfilePath) {
    let relativePath = this.getRelativeProfilePath(aProfilePath);
    let row = (
      await this.#connection.execute(
        "SELECT * FROM Profiles WHERE path = :path;",
        {
          path: relativePath,
        }
      )
    )[0];

    return row ? new SelectableProfile(row) : null;
  }

  // Shared Prefs management

  getPrefValueFromRow(row) {
    let value = row.getResultByName("value");
    if (row.getResultByName("isBoolean")) {
      return value === 1;
    }

    return value;
  }

  /**
   * Get all shared prefs as a list.
   *
   * @returns {{name: string, value: *, type: string}}
   */
  async getAllPrefs() {
    return (
      await this.#connection.executeCached("SELECT * FROM SharedPrefs;")
    ).map(row => {
      let value = this.getPrefValueFromRow(row);
      return {
        name: row.getResultByName("name"),
        value,
        type: typeof value,
      };
    });
  }

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   *
   * @returns {any} Value of the pref
   */
  async getPref(aPrefName) {
    let row = (
      await this.#connection.execute(
        "SELECT value, isBoolean FROM SharedPrefs WHERE name = :name;",
        {
          name: aPrefName,
        }
      )
    )[0];

    return this.getPrefValueFromRow(row);
  }

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   *
   * @returns {boolean} Value of the pref
   */
  async getBoolPref(aPrefName) {
    let prefValue = await this.getPref(aPrefName);
    if (typeof prefValue !== "boolean") {
      return null;
    }

    return prefValue;
  }

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   *
   * @returns {number} Value of the pref
   */
  async getIntPref(aPrefName) {
    let prefValue = await this.getPref(aPrefName);
    if (typeof prefValue !== "number") {
      return null;
    }

    return prefValue;
  }

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   *
   * @returns {string} Value of the pref
   */
  async getStringPref(aPrefName) {
    let prefValue = await this.getPref(aPrefName);
    if (typeof prefValue !== "string") {
      return null;
    }

    return prefValue;
  }

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {any} aPrefValue The value of the pref
   */
  async setPref(aPrefName, aPrefValue) {
    await this.#connection.execute(
      "INSERT INTO SharedPrefs(id, name, value, isBoolean) VALUES (NULL, :name, :value, :isBoolean) ON CONFLICT(name) DO UPDATE SET value=excluded.value, isBoolean=excluded.isBoolean;",
      {
        name: aPrefName,
        value: aPrefValue,
        isBoolean: typeof aPrefValue === "boolean",
      }
    );
  }

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {boolean} aPrefValue The value of the pref
   */
  async setBoolPref(aPrefName, aPrefValue) {
    if (typeof aPrefValue !== "boolean") {
      throw new Error("aPrefValue must be of type boolean");
    }
    await this.setPref(aPrefName, aPrefValue);
  }

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {number} aPrefValue The value of the pref
   */
  async setIntPref(aPrefName, aPrefValue) {
    if (typeof aPrefValue !== "number") {
      throw new Error("aPrefValue must be of type number");
    }
    await this.setPref(aPrefName, aPrefValue);
  }

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {string} aPrefValue The value of the pref
   */
  async setStringPref(aPrefName, aPrefValue) {
    if (typeof aPrefValue !== "string") {
      throw new Error("aPrefValue must be of type string");
    }
    await this.setPref(aPrefName, aPrefValue);
  }

  /**
   * Remove a shared pref, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref to delete
   */
  async deletePref(aPrefName) {
    await this.#connection.execute(
      "DELETE FROM SharedPrefs WHERE name = :name;",
      {
        name: aPrefName,
      }
    );
  }

  // DB lifecycle

  /**
   * Create the SQLite DB for the profile group at groupDBPath.
   * Init shared prefs for the group and add to DB.
   */
  createGroupDB() {}

  /**
   * Vacuum the SQLite DB.
   */
  async vacuumAndCloseGroupDB() {
    await this.#connection.execute("VACUUM;");
    await this.closeConnection();
  }
}

const SelectableProfileService = new SelectableProfileServiceClass();
export { SelectableProfileService };
