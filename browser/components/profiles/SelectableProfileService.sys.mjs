/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfile } from "./SelectableProfile.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CryptoUtils: "resource://services-crypto/utils.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "profilesLocalization", () => {
  return new Localization(["browser/profiles.ftl"], true);
});

const PROFILES_CRYPTO_SALT_LENGTH_BYTES = 16;

function loadImage(url) {
  return new Promise((resolve, reject) => {
    let imageTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
    let imageContainer;
    let observer = imageTools.createScriptedObserver({
      sizeAvailable() {
        resolve(imageContainer);
      },
    });

    imageTools.decodeImageFromChannelAsync(
      url,
      Services.io.newChannelFromURI(
        url,
        null,
        Services.scriptSecurityManager.getSystemPrincipal(),
        null, // aTriggeringPrincipal
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
        Ci.nsIContentPolicy.TYPE_IMAGE
      ),
      (image, status) => {
        if (!Components.isSuccessCode(status)) {
          reject(new Components.Exception("Image loading failed", status));
        } else {
          imageContainer = image;
        }
      },
      observer
    );
  });
}

// This is waiting to be used by bug 1926507.
// eslint-disable-next-line no-unused-vars
async function updateTaskbar(iconUrl, profileName, strokeColor, fillColor) {
  try {
    let image = await loadImage(iconUrl);

    if ("nsIMacDockSupport" in Ci) {
      Cc["@mozilla.org/widget/macdocksupport;1"]
        .getService(Ci.nsIMacDockSupport)
        .setBadgeImage(image, { fillColor, strokeColor });
    } else if ("nsIWinTaskbar" in Ci) {
      lazy.EveryWindow.registerCallback(
        "profiles",
        win => {
          let iconController = Cc["@mozilla.org/windows-taskbar;1"]
            .getService(Ci.nsIWinTaskbar)
            .getOverlayIconController(win.docShell);
          iconController.setOverlayIcon(image, profileName, {
            fillColor,
            strokeColor,
          });
        },
        () => {}
      );
    }
  } catch (e) {
    console.error(e);
  }
}

/**
 * The service that manages selectable profiles
 */
class SelectableProfileServiceClass {
  #profileService = null;
  #connection = null;
  #asyncShutdownBlocker = null;
  #initialized = false;
  #groupToolkitProfile = null;
  #storeID = null;
  #currentProfile = null;
  #everyWindowCallbackId = "SelectableProfileService";
  #defaultAvatars = [
    "book",
    "briefcase",
    "flower",
    "heart",
    "shopping",
    "star",
  ];
  #initPromise = null;
  static #dirSvc = null;

  constructor() {
    this.themeObserver = this.themeObserver.bind(this);
    this.#profileService = Cc[
      "@mozilla.org/toolkit/profile-service;1"
    ].getService(Ci.nsIToolkitProfileService);

    this.#groupToolkitProfile =
      this.#profileService.currentProfile ?? this.#profileService.groupProfile;

    this.#storeID = this.#groupToolkitProfile?.storeID;

    if (!this.#storeID) {
      this.#storeID = Services.prefs.getCharPref(
        "toolkit.profiles.storeID",
        ""
      );
      if (this.#storeID) {
        // This can happen if profiles.ini has been reset by a version of Firefox prior to 67 and
        // the current profile is not the current default for the group. We can recover by
        // attempting to find the group profile from the database.
        this.#initPromise = this.restoreStoreID()
          .catch(console.error)
          .finally(() => (this.#initPromise = null));
      }
    }
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
    this.#groupToolkitProfile =
      this.#profileService.currentProfile ?? this.#profileService.groupProfile;
    this.#storeID = this.#groupToolkitProfile?.storeID;
    await this.init();
  }

  overrideDirectoryService(dirSvc) {
    if (!Cu.isInAutomation) {
      return;
    }

    SelectableProfileServiceClass.#dirSvc = dirSvc;
  }

  static getDirectory(id) {
    if (this.#dirSvc) {
      if (id in this.#dirSvc) {
        return this.#dirSvc[id];
      }
    }

    return Services.dirsvc.get(id, Ci.nsIFile);
  }

  async #attemptFlushProfileService() {
    try {
      await this.#profileService.asyncFlush();
    } catch (e) {
      try {
        await this.#profileService.asyncFlushCurrentProfile();
      } catch (ex) {
        console.error(
          `Failed to flush changes to the profiles database: ${ex}`
        );
      }
    }
  }

  get storeID() {
    return this.#storeID;
  }

  get groupToolkitProfile() {
    return this.#groupToolkitProfile;
  }

  get currentProfile() {
    return this.#currentProfile;
  }

  get initialized() {
    return this.#initialized;
  }

  static get PROFILE_GROUPS_DIR() {
    if (this.#dirSvc && "ProfileGroups" in this.#dirSvc) {
      return this.#dirSvc.ProfileGroups;
    }

    return PathUtils.join(this.getDirectory("UAppData").path, "Profile Groups");
  }

  async maybeCreateProfilesStorePath() {
    if (this.storeID) {
      return;
    }

    await IOUtils.makeDirectory(
      SelectableProfileServiceClass.PROFILE_GROUPS_DIR
    );

    const storageID = Services.uuid
      .generateUUID()
      .toString()
      .replace("{", "")
      .split("-")[0];
    this.#groupToolkitProfile.storeID = storageID;
    this.#storeID = storageID;
    await this.#attemptFlushProfileService();
  }

  async getProfilesStorePath() {
    await this.maybeCreateProfilesStorePath();

    return PathUtils.join(
      SelectableProfileServiceClass.PROFILE_GROUPS_DIR,
      `${this.storeID}.sqlite`
    );
  }

  /**
   * At startup, store the nsToolkitProfile for the group.
   * Get the groupDBPath from the nsToolkitProfile, and connect to it.
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
    if (this.#initialized || !this.groupToolkitProfile) {
      return;
    }

    // If the storeID doesn't exist, we don't want to create the db until we
    // need to so we early return.
    if (!this.storeID) {
      return;
    }

    await this.initConnection();

    // When we launch into the startup window, the `ProfD` is not defined so
    // getting the directory will throw. Leaving the `currentProfile` as null
    // is fine for the startup window.
    try {
      // Get the SelectableProfile by the profile directory
      this.#currentProfile = await this.getProfileByPath(
        SelectableProfileServiceClass.getDirectory("ProfD")
      );
    } catch {}

    this.setSharedPrefs();

    // The 'activate' event listeners use #currentProfile, so this line has
    // to come after #currentProfile has been set.
    this.initWindowTracker();

    Services.obs.addObserver(
      this.themeObserver,
      "lightweight-theme-styling-update"
    );

    this.#initialized = true;
  }

  async uninit() {
    if (!this.#initialized) {
      return;
    }

    lazy.EveryWindow.unregisterCallback(this.#everyWindowCallbackId);

    Services.obs.removeObserver(
      this.themeObserver,
      "lightweight-theme-styling-update"
    );

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

  async restoreStoreID() {
    try {
      await this.#init();

      for (let profile of await this.getAllProfiles()) {
        let groupProfile = this.#profileService.getProfileByDir(
          await profile.rootDir
        );

        if (groupProfile) {
          this.#groupToolkitProfile = groupProfile;
          this.#groupToolkitProfile.storeID = this.storeID;
          await this.#profileService.asyncFlush();
          return;
        }
      }
    } catch (e) {
      console.error(e);
    }

    // If we were unable to find a matching toolkit profile then assume the
    // store ID is bogus so clear it and uninit.
    this.#storeID = null;
    await this.uninit();
    Services.prefs.clearUserPref("toolkit.profiles.storeID");
  }

  async handleEvent(event) {
    switch (event.type) {
      case "activate": {
        this.setDefaultProfileForGroup();
        break;
      }
    }
  }

  /**
   * Set the shared prefs that will be needed when creating a
   * new selectable profile.
   */
  setSharedPrefs() {
    this.setPref(
      "toolkit.profiles.storeID",
      Services.prefs.getStringPref("toolkit.profiles.storeID", "")
    );
    this.setPref(
      "browser.profiles.enabled",
      Services.prefs.getBoolPref("browser.profiles.enabled", true)
    );
    this.setPref(
      "toolkit.telemetry.cachedProfileGroupID",
      Services.prefs.getStringPref("toolkit.telemetry.cachedProfileGroupID", "")
    );
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
    if ((await this.getAllProfiles()).length) {
      return;
    }

    this.#groupToolkitProfile.storeID = null;
    this.#storeID = null;
    await this.#attemptFlushProfileService();
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
    let executable = SelectableProfileServiceClass.getDirectory("XREExeF");
    process.init(executable);
    return process;
  }

  /**
   * Launch a new Firefox instance using the given selectable profile.
   *
   * @param {SelectableProfile} aProfile The profile to launch
   * @param {string} url A url to open in launched profile
   */
  launchInstance(aProfile, url) {
    let process = this.getExecutableProcess();
    let args = ["--profile", aProfile.path];
    if (Services.appinfo.OS === "Darwin") {
      args.unshift("-foreground");
    }
    if (url) {
      args.push("-url", url);
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
   * The observer function that watches for theme changes and updates the
   * current profile of a theme change.
   *
   * @param {object} aSubject The theme data
   * @param {*} aTopic Should be "lightweight-theme-styling-update"
   */
  themeObserver(aSubject, aTopic) {
    if (aTopic !== "lightweight-theme-styling-update") {
      return;
    }

    let data = aSubject.wrappedJSObject;

    let theme = data.theme;
    this.currentProfile.theme = {
      themeL10nId: theme.id,
      themeFg: theme.textcolor,
      themeBg: theme.accentcolor,
    };
  }

  /**
   * Init or update the current SelectableProfiles from the DB.
   */
  refreshProfiles() {}

  /**
   * Fetch all prefs from the DB and write to the current instance.
   */
  refreshPrefs() {}

  /**
   * Update the default profile by setting the current selectable profile path
   * as the path of the nsToolkitProfile for the group.
   */
  async setDefaultProfileForGroup() {
    if (
      !this.currentProfile ||
      this.#groupToolkitProfile.rootDir.path === this.currentProfile.path
    ) {
      return;
    }
    this.#groupToolkitProfile.rootDir = await this.currentProfile.rootDir;
    await this.#attemptFlushProfileService();
  }

  /**
   * Update whether to show the selectable profile selector window at startup.
   * Set on the nsToolkitProfile instance for the group.
   *
   * @param {boolean} shouldShow Whether or not we should show the profile selector
   */
  async showProfileSelectorWindow(shouldShow) {
    if (shouldShow === this.groupToolkitProfile.showProfileSelector) {
      return;
    }

    this.groupToolkitProfile.showProfileSelector = shouldShow;
    await this.#attemptFlushProfileService();
  }

  // SelectableProfile lifecycle

  /**
   * Create the profile directory for new profile. The profile name is combined
   * with a salt string to ensure the directory is unique. The format of the
   * directory is salt + "." + profileName. (Ex. c7IZaLu7.testProfile)
   *
   * @param {string} aProfileName The name of the profile to be created
   * @returns {string} The path for the given profile
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
          SelectableProfileServiceClass.getDirectory("DefProfRt").path,
          profileDir
        )
      ),
      IOUtils.makeDirectory(
        PathUtils.join(
          SelectableProfileServiceClass.getDirectory("DefProfLRt").path,
          profileDir
        )
      ),
    ]);

    return IOUtils.getDirectory(
      PathUtils.join(
        SelectableProfileServiceClass.getDirectory("DefProfRt").path,
        profileDir
      )
    );
  }

  /**
   * Create the times.json file and write the "created" timestamp and
   * "firstUse" as null.
   * Create the prefs.js file and write all shared prefs to the file.
   *
   * @param {nsIFile} profileDir The root dir of the newly created profile
   */
  async createProfileInitialFiles(profileDir) {
    let timesJsonFilePath = await IOUtils.createUniqueFile(
      profileDir.path,
      "times.json",
      0o700
    );

    await IOUtils.writeJSON(timesJsonFilePath, {
      created: Date.now(),
      firstUse: null,
    });

    let prefsJsFilePath = await IOUtils.createUniqueFile(
      profileDir.path,
      "prefs.js",
      0o700
    );

    const sharedPrefs = await this.getAllPrefs();

    const LINEBREAK = AppConstants.platform === "win" ? "\r\n" : "\n";

    const prefsJsHeader = [
      "// Mozilla User Preferences",
      LINEBREAK,
      "// DO NOT EDIT THIS FILE.",
      "//",
      "// If you make changes to this file while the application is running,",
      "// the changes will be overwritten when the application exits.",
      "//",
      "// To change a preference value, you can either:",
      "// - modify it via the UI (e.g. via about:config in the browser); or",
      "// - set it within a user.js file in your profile.",
      LINEBREAK,
    ];

    const prefsJsContent = sharedPrefs.map(
      pref =>
        `user_pref("${pref.name}", ${
          pref.type === "string" ? `"${pref.value}"` : `${pref.value}`
        });`
    );

    const prefsJs = prefsJsHeader.concat(
      prefsJsContent,
      'user_pref("browser.profiles.profile-name.updated", false);'
    );

    await IOUtils.writeUTF8(prefsJsFilePath, prefsJs.join(LINEBREAK));
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
      SelectableProfileServiceClass.getDirectory("UAppData")
    );

    if (AppConstants.platform === "win") {
      relativePath = relativePath.replace("/", "\\");
    }

    return relativePath;
  }

  /**
   * Create a Selectable Profile and add to the datastore.
   *
   * If path is not included, new profile directories will be created.
   *
   * @param {nsIFile} existingProfilePath Optional. The path of an existing profile.
   *
   * @returns {SelectableProfile} The newly created profile object.
   */
  async #createProfile(existingProfilePath) {
    let nextProfileNumber =
      1 + Math.max(0, ...(await this.getAllProfiles()).map(p => p.id));
    let [defaultName] = lazy.profilesLocalization.formatMessagesSync([
      { id: "default-profile-name", args: { number: nextProfileNumber } },
    ]);
    let randomIndex = Math.floor(Math.random() * this.#defaultAvatars.length);
    let profileData = {
      name: defaultName.value,
      avatar: this.#defaultAvatars[randomIndex],
      themeL10nId: "default",
      themeFg: "var(--text-color)",
      themeBg: "var(--background-color-box)",
    };

    let path =
      existingProfilePath || (await this.createProfileDirs(profileData.name));
    if (!existingProfilePath) {
      await this.createProfileInitialFiles(path);
    }
    profileData.path = this.getRelativeProfilePath(path);

    let profile = await this.insertProfile(profileData);
    return profile;
  }

  /**
   * If the user has never created a SelectableProfile before, the group
   * datastore will be created and the currently running toolkit profile will
   * be added to the datastore.
   */
  async maybeSetupDataStore() {
    // Create the profiles db and set the storeID on the toolkit profile if it
    // doesn't exist so we can init the service.
    await this.maybeCreateProfilesStorePath();
    await this.init();

    // If this is the first time the user has created a selectable profile,
    // add the current toolkit profile to the datastore.
    let profiles = await this.getAllProfiles();
    if (!profiles.length) {
      let path = this.#profileService.currentProfile.rootDir;
      this.#currentProfile = await this.#createProfile(path);
    }
  }

  /**
   * Create and launch a new SelectableProfile and add it to the group datastore.
   * This is an unmanaged profile from the nsToolkitProfile perspective.
   *
   * If the user has never created a SelectableProfile before, the group
   * datastore will be lazily created and the currently running toolkit profile
   * will be added to the datastore along with the newly created profile.
   *
   * Launches the new SelectableProfile in a new instance after creating it.
   */
  async createNewProfile() {
    await this.maybeSetupDataStore();

    let profile = await this.#createProfile();
    this.launchInstance(profile);
  }

  /**
   * Add a profile to the profile group datastore.
   *
   * This function assumes the service is initialized and the datastore has
   * been created.
   *
   * @param {object} profileData A plain object that contains a name, avatar,
   *                 themeL10nId, themeFg, themeBg, and relative path as string.
   *
   * @returns {SelectableProfile} The newly created profile object.
   */
  async insertProfile(profileData) {
    // Verify all fields are present.
    let keys = ["avatar", "name", "path", "themeBg", "themeFg", "themeL10nId"];
    let missing = [];
    keys.forEach(key => {
      if (!(key in profileData)) {
        missing.push(key);
      }
    });
    if (missing.length) {
      throw new Error(
        "Unable to insertProfile due to missing keys: ",
        missing.join(",")
      );
    }
    await this.#connection.execute(
      `INSERT INTO Profiles VALUES (NULL, :path, :name, :avatar, :themeL10nId, :themeFg, :themeBg);`,
      profileData
    );
    return this.getProfileByName(profileData.name);
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
          SelectableProfileServiceClass.getDirectory("DefProfRt").path,
          profileDir
        ),
        {
          recursive: true,
        }
      ),
      IOUtils.remove(
        PathUtils.join(
          SelectableProfileServiceClass.getDirectory("DefProfLRt").path,
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
    let profileObj = aSelectableProfile.toObject();

    await this.#connection.execute(
      `UPDATE Profiles
       SET path = :path, name = :name, avatar = :avatar, themeL10nId = :themeL10nId, themeFg = :themeFg, themeBg = :themeBg
       WHERE id = :id;`,
      profileObj
    );
  }

  /**
   * Get the complete list of profiles in the group.
   *
   * @returns {Array<SelectableProfile>}
   *   An array of profiles in the group.
   */
  async getAllProfiles() {
    if (!this.#connection) {
      return [];
    }

    return (
      await this.#connection.executeCached("SELECT * FROM Profiles;")
    ).map(row => {
      return new SelectableProfile(row, this);
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
    if (!this.#connection) {
      return null;
    }

    let row = (
      await this.#connection.execute("SELECT * FROM Profiles WHERE id = :id;", {
        id: aProfileID,
      })
    )[0];

    return row ? new SelectableProfile(row, this) : null;
  }

  /**
   * Get a specific profile by its name.
   *
   * @param {string} aProfileNanme The name of the profile
   * @returns {SelectableProfile}
   *   The specific profile.
   */
  async getProfileByName(aProfileNanme) {
    if (!this.#connection) {
      return null;
    }

    let row = (
      await this.#connection.execute(
        "SELECT * FROM Profiles WHERE name = :name;",
        {
          name: aProfileNanme,
        }
      )
    )[0];

    return row ? new SelectableProfile(row, this) : null;
  }

  /**
   * Get a specific profile by its absolute path.
   *
   * @param {nsIFile} aProfilePath The path of the profile
   * @returns {SelectableProfile|null}
   */
  async getProfileByPath(aProfilePath) {
    if (!this.#connection) {
      return null;
    }

    let relativePath = this.getRelativeProfilePath(aProfilePath);
    let row = (
      await this.#connection.execute(
        "SELECT * FROM Profiles WHERE path = :path;",
        {
          path: relativePath,
        }
      )
    )[0];

    return row ? new SelectableProfile(row, this) : null;
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
