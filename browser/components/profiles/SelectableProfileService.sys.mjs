/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TDOD: Remove eslint-disable lines once methods are updated. See bug 1896727
/* eslint-disable no-unused-vars */
/* eslint-disable no-unused-private-class-members */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

function getProfileGroupsDir() {
  return PathUtils.join(
    Services.dirsvc.get("UAppData", Ci.nsIFile).path,
    "Profile Groups"
  );
}

/**
 * The service that manages selectable profiles
 */
export class SelectableProfileService {
  #connection = null;
  #asyncShutdownBlocker = null;
  #initialized = false;
  #groupToolkitProfile = null;

  async createProfilesStorePath() {
    await IOUtils.makeDirectory(getProfileGroupsDir());

    const storageID = Services.uuid
      .generateUUID()
      .toString()
      .replace("{", "")
      .split("-")[0];
    this.#groupToolkitProfile.storeID = storageID;
  }

  async getProfilesStorePath() {
    if (!this.#groupToolkitProfile.storeID) {
      await this.createProfilesStorePath();
    }

    return PathUtils.join(
      getProfileGroupsDir(),
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

    this.#groupToolkitProfile = lazy.ProfileService.currentProfile;

    await this.initConnection();

    this.#initialized = true;
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
    if (this.getProfiles().length) {
      return;
    }

    this.#groupToolkitProfile.storeID = null;
    await this.vacuumAndCloseGroupDB();
  }

  // App session lifecycle methods and multi-process support

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
  setShowProfileChooser(shouldShow) {}

  /**
   * Update the path to the group DB. Set on the nsToolkitProfile instance
   * for the group.
   *
   * @param {string} aPath The path to the group DB
   */
  setGroupDBPath(aPath) {}

  // SelectableProfile lifecycle

  /**
   * Create an empty SelectableProfile and add it to the group DB.
   * This is an unmanaged profile from the nsToolkitProfile perspective.
   */
  createProfile() {}

  /**
   * Delete a SelectableProfile from the group DB.
   * If it was the last profile in the group, also call deleteProfileGroup().
   */
  deleteProfile() {}

  /**
   * Schedule deletion of the current SelectableProfile as a background task, then exit.
   */
  deleteCurrentProfile() {}

  /**
   * Write an updated profile to the DB.
   *
   * @param {SelectableProfile} aSelectableProfile The SelectableProfile to be update
   */
  updateProfile(aSelectableProfile) {}

  /**
   * Get the complete list of profiles in the group.
   */
  getProfiles() {}

  /**
   * Get a specific profile by its internal ID.
   *
   * @param {number} aProfileID The internal id of the profile
   */
  getProfile(aProfileID) {}

  // Shared Prefs management

  /**
   * Get all shared prefs as a list.
   */
  getAllPrefs() {}

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   */
  getPref(aPrefName) {}

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {string} aPrefType The type of the pref
   * @param {any} aPrefValue The value of the pref
   */
  createOrUpdatePref(aPrefName, aPrefType, aPrefValue) {}

  /**
   * Remove a shared pref, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref to delete
   */
  deletePref(aPrefName) {}

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
