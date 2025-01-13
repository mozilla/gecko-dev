/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SelectableProfile } from "./SelectableProfile.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { DeferredTask } from "resource://gre/modules/DeferredTask.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

// This is used to keep the icon controllers alive for as long as their windows are alive.
const TASKBAR_ICON_CONTROLLERS = new WeakMap();

ChromeUtils.defineESModuleGetters(lazy, {
  CryptoUtils: "resource://services-crypto/utils.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "profilesLocalization", () => {
  return new Localization(["browser/profiles.ftl"], true);
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "PROFILES_ENABLED",
  "browser.profiles.enabled",
  false
);

const PROFILES_CRYPTO_SALT_LENGTH_BYTES = 16;
const NOTIFY_TIMEOUT = 200;

const COMMAND_LINE_UPDATE = "profiles-updated";
const COMMAND_LINE_ACTIVATE = "profiles-activate";

const gSupportsBadging = "nsIMacDockSupport" in Ci || "nsIWinTaskbar" in Ci;

function loadImage(url) {
  return new Promise((resolve, reject) => {
    let imageTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
    let imageContainer;
    let observer = imageTools.createScriptedObserver({
      sizeAvailable() {
        resolve(imageContainer);
        imageContainer = null;
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
  #notifyTask = null;
  #observedPrefs = null;
  #badge = null;
  static #dirSvc = null;

  // The initial preferences that will be shared amongst profiles. Only used during database
  // creation, after that the set in the database is used.
  static initialSharedPrefs = ["toolkit.telemetry.cachedProfileGroupID"];
  // Preferences that were previously shared but should now be ignored.
  static ignoredSharedPrefs = [
    "browser.profiles.enabled",
    "toolkit.profiles.storeID",
  ];

  constructor() {
    this.themeObserver = this.themeObserver.bind(this);
    this.prefObserver = (subject, topic, prefName) =>
      this.flushSharedPrefToDatabase(prefName);
    this.#profileService = Cc[
      "@mozilla.org/toolkit/profile-service;1"
    ].getService(Ci.nsIToolkitProfileService);

    this.#asyncShutdownBlocker = () => this.uninit();
    this.#observedPrefs = new Set();
  }

  get isEnabled() {
    // If a storeID has been assigned then profiles may have been created so force us on. Also
    // covers the case when the selector is shown at startup and we don't have preferences
    // available.
    if (this.storeID) {
      return true;
    }

    return lazy.PROFILES_ENABLED && !!this.#groupToolkitProfile;
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

    let enabled = lazy.PROFILES_ENABLED;
    if (enabled) {
      // Various parts of the UI listen to the pref to trigger updating so toggle it here.
      Services.prefs.setBoolPref("browser.profiles.enabled", false);
      Services.prefs.setBoolPref("browser.profiles.enabled", true);
    }
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
        return this.#dirSvc[id].clone();
      }
    }

    return Services.dirsvc.get(id, Ci.nsIFile);
  }

  async #attemptFlushProfileService() {
    try {
      await this.#profileService.asyncFlush();
    } catch (e) {
      try {
        await this.#profileService.asyncFlushGroupProfile();
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

    if (!this.#groupToolkitProfile) {
      throw new Error("Cannot create a store without a group profile.");
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
    if (this.#initialized) {
      return;
    }

    this.#groupToolkitProfile =
      this.#profileService.currentProfile ?? this.#profileService.groupProfile;
    this.#storeID = this.#groupToolkitProfile?.storeID;

    if (!this.storeID) {
      this.#storeID = Services.prefs.getCharPref(
        "toolkit.profiles.storeID",
        ""
      );
    }

    if (!this.isEnabled) {
      return;
    }

    // If the storeID doesn't exist, we don't want to create the db until we
    // need to so we early return.
    if (!this.storeID) {
      return;
    }

    // This could fail if we're adding it during shutdown. In this case,
    // don't throw but don't continue initialization.
    try {
      lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
        "SelectableProfileService uninit",
        this.#asyncShutdownBlocker
      );
    } catch (ex) {
      console.error(ex);
      return;
    }

    this.#notifyTask = new DeferredTask(async () => {
      // Notify ourselves.
      await this.databaseChanged("local");
      // Notify other instances.
      await this.#notifyRunningInstances();
    }, NOTIFY_TIMEOUT);

    try {
      await this.initConnection();
    } catch (e) {
      console.error(e);

      // If this was an attempt to recover the storeID then reset it.
      if (!this.#groupToolkitProfile?.storeID) {
        Services.prefs.clearUserPref("toolkit.profiles.storeID");
      }

      await this.uninit();
      return;
    }

    // This can happen if profiles.ini has been reset by a version of Firefox
    // prior to 67 and the current profile is not the current default for the
    // group. We can recover by attempting to find the group profile from the
    // database.
    if (this.#groupToolkitProfile?.storeID != this.storeID) {
      await this.#restoreStoreID();

      if (!this.#groupToolkitProfile) {
        // If we were unable to find a matching toolkit profile then assume the
        // store ID is bogus so clear it and uninit.
        Services.prefs.clearUserPref("toolkit.profiles.storeID");
        await this.uninit();
        return;
      }
    }

    // When we launch into the startup window, the `ProfD` is not defined so
    // getting the directory will throw. Leaving the `currentProfile` as null
    // is fine for the startup window.
    try {
      // Get the SelectableProfile by the profile directory
      this.#currentProfile = await this.getProfileByPath(
        SelectableProfileServiceClass.getDirectory("ProfD")
      );
    } catch {}

    // The 'activate' event listeners use #currentProfile, so this line has
    // to come after #currentProfile has been set.
    this.initWindowTracker();

    Services.obs.addObserver(
      this.themeObserver,
      "lightweight-theme-styling-update"
    );

    this.#initialized = true;

    // this.#currentProfile is unset in the case that the database has only just been created. We
    // don't need to import from the database in this case.
    if (this.#currentProfile) {
      // Assume that settings in the database may have changed while we weren't running.
      await this.databaseChanged("startup");
    }
  }

  async uninit() {
    if (!this.#initialized) {
      return;
    }

    lazy.AsyncShutdown.profileChangeTeardown.removeBlocker(
      this.#asyncShutdownBlocker
    );

    try {
      Services.obs.removeObserver(
        this.themeObserver,
        "lightweight-theme-styling-update"
      );
    } catch (e) {}

    for (let prefName of this.#observedPrefs) {
      Services.prefs.removeObserver(prefName, this.prefObserver);
    }
    this.#observedPrefs.clear();

    // During shutdown we don't need to notify ourselves, just other instances
    // so rather than finalizing the task just disarm it and do the notification
    // manually.
    if (this.#notifyTask.isArmed) {
      this.#notifyTask.disarm();
      await this.#notifyRunningInstances();
    }

    await this.closeConnection();

    this.#currentProfile = null;
    this.#groupToolkitProfile = null;
    this.#storeID = null;
    this.#badge = null;

    lazy.EveryWindow.unregisterCallback(this.#everyWindowCallbackId);

    this.#initialized = false;
  }

  initWindowTracker() {
    lazy.EveryWindow.registerCallback(
      this.#everyWindowCallbackId,
      window => {
        if (this.#badge && "nsIWinTaskbar" in Ci) {
          let iconController = Cc["@mozilla.org/windows-taskbar;1"]
            .getService(Ci.nsIWinTaskbar)
            .getOverlayIconController(window.docShell);
          TASKBAR_ICON_CONTROLLERS.set(window, iconController);

          iconController.setOverlayIcon(
            this.#badge.image,
            this.#badge.description,
            this.#badge.iconPaintContext
          );
        }

        // Update the window title because the currentProfile, needed in the
        // .*-with-profile titles, didn't exist when the title was initially set.
        window.gBrowser.updateTitlebar();

        let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(window);
        if (isPBM) {
          return;
        }

        window.addEventListener("activate", this);
      },
      window => {
        window.gBrowser.updateTitlebar();

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

    await this.createProfilesDBTables();
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

  async #restoreStoreID() {
    try {
      // Finds the first nsIToolkitProfile that matches the path of a
      // SelectableProfile in the database.
      for (let profile of await this.getAllProfiles()) {
        let groupProfile = this.#profileService.getProfileByDir(
          await profile.rootDir
        );

        if (groupProfile && !groupProfile.storeID) {
          groupProfile.storeID = this.storeID;
          await this.#profileService.asyncFlush();
          this.#groupToolkitProfile = groupProfile;
          return;
        }
      }
    } catch (e) {
      console.error(e);
    }
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
   * Flushes the value of a preference to the database.
   *
   * @param {string} prefName the name of the preference.
   */
  async flushSharedPrefToDatabase(prefName) {
    if (!this.#observedPrefs.has(prefName)) {
      Services.prefs.addObserver(prefName, this.prefObserver);
      this.#observedPrefs.add(prefName);
    }

    if (!Services.prefs.prefHasUserValue(prefName)) {
      await this.#deleteDBPref(prefName);
      return;
    }

    let value;

    switch (Services.prefs.getPrefType(prefName)) {
      case Ci.nsIPrefBranch.PREF_BOOL:
        value = Services.prefs.getBoolPref(prefName);
        break;
      case Ci.nsIPrefBranch.PREF_INT:
        value = Services.prefs.getIntPref(prefName);
        break;
      case Ci.nsIPrefBranch.PREF_STRING:
        value = Services.prefs.getCharPref(prefName);
        break;
    }

    await this.#setDBPref(prefName, value);
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
    } else {
      args.push(`--${COMMAND_LINE_ACTIVATE}`);
    }
    process.runw(false, args, args.length);
  }

  /**
   * When the group DB has been updated, either changes to prefs or profiles,
   * ask the remoting service to notify other running instances that they should
   * check for updates and refresh their UI accordingly.
   */
  async #notifyRunningInstances() {
    let remoteService = Cc["@mozilla.org/remote;1"].getService(
      Ci.nsIRemoteService
    );

    let profiles = await this.getAllProfiles();
    for (let profile of profiles) {
      // The current profile was notified above.
      if (profile.id === this.#currentProfile?.id) {
        continue;
      }

      try {
        remoteService.sendCommandLine(
          profile.path,
          [`--${COMMAND_LINE_UPDATE}`],
          false
        );
      } catch (e) {
        // This is expected to fail if no instance is running with the profile.
      }
    }
  }

  async #updateTaskbar() {
    try {
      // We don't want the startup profile selector to badge the dock icon.
      if (!gSupportsBadging || Services.startup.startingUp) {
        return;
      }

      let count = await this.getProfileCount();

      if (count > 1 && !this.#badge) {
        this.#badge = {
          image: await loadImage(
            Services.io.newURI(
              `chrome://browser/content/profiles/assets/48_${
                this.#currentProfile.avatar
              }.svg`
            )
          ),
          iconPaintContext: this.#currentProfile.iconPaintContext,
          description: this.#currentProfile.name,
        };

        if ("nsIMacDockSupport" in Ci) {
          Cc["@mozilla.org/widget/macdocksupport;1"]
            .getService(Ci.nsIMacDockSupport)
            .setBadgeImage(this.#badge.image, this.#badge.iconPaintContext);
        } else if ("nsIWinTaskbar" in Ci) {
          for (let win of lazy.EveryWindow.readyWindows) {
            let iconController = Cc["@mozilla.org/windows-taskbar;1"]
              .getService(Ci.nsIWinTaskbar)
              .getOverlayIconController(win.docShell);
            TASKBAR_ICON_CONTROLLERS.set(win, iconController);

            iconController.setOverlayIcon(
              this.#badge.image,
              this.#badge.description,
              this.#badge.iconPaintContext
            );
          }
        }
      } else if (count <= 1 && this.#badge) {
        this.#badge = null;

        if ("nsIMacDockSupport" in Ci) {
          Cc["@mozilla.org/widget/macdocksupport;1"]
            .getService(Ci.nsIMacDockSupport)
            .setBadgeImage(null);
        } else if ("nsIWinTaskbar" in Ci) {
          for (let win of lazy.EveryWindow.readyWindows) {
            let iconController = TASKBAR_ICON_CONTROLLERS.get(win);
            iconController?.setOverlayIcon(null, null);
          }
        }
      }
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Invoked when changes have been made to the database. Sends the observer
   * notification "sps-profiles-updated" indicating that something has changed.
   *
   * @param {"local"|"remote"|"startup"} source The source of the notification.
   *   Either "local" meaning that the change was made in this process, "remote"
   *   meaning the change was made by a different Firefox instance or "startup"
   *   meaning the application has just launched and we may need to reload
   *   changes from the database.
   */
  async databaseChanged(source) {
    if (source != "local") {
      await this.loadSharedPrefsFromDatabase();
    }

    await this.#updateTaskbar();

    if (source != "startup") {
      Services.obs.notifyObservers(null, "sps-profiles-updated", source);
    }
  }

  /**
   * The observer function that watches for theme changes and updates the
   * current profile of a theme change.
   *
   * @param {object} aSubject The theme data
   * @param {string} aTopic Should be "lightweight-theme-styling-update"
   */
  themeObserver(aSubject, aTopic) {
    if (aTopic !== "lightweight-theme-styling-update") {
      return;
    }

    let data = aSubject.wrappedJSObject;

    if (!data.theme) {
      // During startup the theme might be null so just return
      return;
    }

    let window = Services.wm.getMostRecentBrowserWindow();
    let isDark = window.matchMedia("(-moz-system-dark-theme)").matches;

    let theme = isDark && !!data.darkTheme ? data.darkTheme : data.theme;

    let themeFg = theme.toolbar_text;
    let themeBg = theme.toolbarColor;

    if (!themeFg || !themeBg) {
      // TODO Bug 1927193: The colors defined below are from the light and
      // dark theme manifest files and they are not accurate for the default
      // theme. We should read the color values from the document to get the
      // correct colors.
      const defaultDarkText = "rgb(255,255,255)"; // dark theme "tab_text"
      const defaultLightText = "rgb(21,20,26)"; // light theme "tab_text"
      const defaultDarkToolbar = "rgb(43,42,51)"; // dark theme "toolbar"
      const defaultLightToolbar = "#f9f9fb"; // light theme "toolbar"

      themeFg = isDark ? defaultDarkText : defaultLightText;
      themeBg = isDark ? defaultDarkToolbar : defaultLightToolbar;
    }

    this.currentProfile.theme = {
      themeId: theme.id,
      themeFg,
      themeBg,
    };
  }

  /**
   * Fetch all prefs from the DB and write to the current instance.
   */
  async loadSharedPrefsFromDatabase() {
    // This stops us from observing the change during the load and means we stop observing any prefs
    // no longer in the database.
    for (let prefName of this.#observedPrefs) {
      Services.prefs.removeObserver(prefName, this.prefObserver);
    }
    this.#observedPrefs.clear();

    for (let { name, value, type } of await this.getAllDBPrefs()) {
      if (SelectableProfileServiceClass.ignoredSharedPrefs.includes(name)) {
        continue;
      }

      if (value === null) {
        Services.prefs.clearUserPref(name);
      } else {
        switch (type) {
          case "boolean":
            Services.prefs.setBoolPref(name, value);
            break;
          case "string":
            Services.prefs.setCharPref(name, value);
            break;
          case "number":
            Services.prefs.setIntPref(name, value);
            break;
          case "null":
            Services.prefs.clearUserPref(name);
            break;
        }
      }

      Services.prefs.addObserver(name, this.prefObserver);
      this.#observedPrefs.add(name);
    }
  }

  /**
   * Update the default profile by setting the selectable profile's path
   * as the path of the nsToolkitProfile for the group. Defaults to the current
   * selectable profile.
   *
   * @param {SelectableProfile} aProfile The SelectableProfile to be
   * set as the default.
   */
  async setDefaultProfileForGroup(aProfile = this.currentProfile) {
    if (!aProfile) {
      return;
    }
    this.#groupToolkitProfile.rootDir = await aProfile.rootDir;
    await this.#attemptFlushProfileService();
    Glean.profilesDefault.updated.record();
  }

  /**
   * Update whether to show the selectable profile selector window at startup.
   * Set on the nsToolkitProfile instance for the group.
   *
   * @param {boolean} shouldShow Whether or not we should show the profile selector
   */
  async showProfileSelectorWindow(shouldShow) {
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
      0o600
    );

    const sharedPrefs = await this.getAllDBPrefs();

    const LINEBREAK = AppConstants.platform === "win" ? "\r\n" : "\n";

    const prefsJs = [
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
      'user_pref("browser.profiles.profile-name.updated", false);',
    ];

    for (let pref of sharedPrefs) {
      prefsJs.push(
        `user_pref("${pref.name}", ${
          pref.type === "string" ? `"${pref.value}"` : `${pref.value}`
        });`
      );
    }

    // Preferences that must be set in newly created profiles.
    prefsJs.push(`user_pref("browser.profiles.enabled", true);`);
    prefsJs.push(`user_pref("toolkit.profiles.storeID", "${this.storeID}");`);

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
      relativePath = relativePath.replaceAll("/", "\\");
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
    let nextProfileNumber = Math.max(
      0,
      ...(await this.getAllProfiles()).map(p => p.id)
    );
    let [defaultName, originalName] =
      lazy.profilesLocalization.formatMessagesSync([
        { id: "default-profile-name", args: { number: nextProfileNumber } },
        { id: "original-profile-name" },
      ]);

    let window = Services.wm.getMostRecentBrowserWindow();
    let isDark = window?.matchMedia("(-moz-system-dark-theme)").matches;

    let randomIndex = Math.floor(Math.random() * this.#defaultAvatars.length);
    let profileData = {
      // The original toolkit profile is added first and is assigned a
      // different name.
      name: nextProfileNumber == 0 ? originalName.value : defaultName.value,
      avatar: this.#defaultAvatars[randomIndex],
      themeId: "default-theme@mozilla.org",
      themeFg: isDark ? "rgb(255,255,255)" : "rgb(21,20,26)",
      themeBg: isDark ? "rgb(28, 27, 34)" : "rgb(240, 240, 244)",
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
    if (this.#connection) {
      return;
    }

    // Create the profiles db and set the storeID on the toolkit profile if it
    // doesn't exist so we can init the service.
    await this.maybeCreateProfilesStorePath();
    await this.init();

    // Flush our shared prefs into the database.
    for (let prefName of SelectableProfileServiceClass.initialSharedPrefs) {
      await this.flushSharedPrefToDatabase(prefName);
    }

    // If this is the first time the user has created a selectable profile,
    // add the current toolkit profile to the datastore.
    if (!this.#currentProfile) {
      let path = this.#profileService.currentProfile.rootDir;
      this.#currentProfile = await this.#createProfile(path);

      // And also set the profile selector window to show at startup (bug 1933911).
      this.showProfileSelectorWindow(true);
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
   *
   * @param {boolean} [launchProfile=true] Whether or not this should launch
   * the newly created profile.
   *
   * @returns {SelectableProfile} The profile just created.
   */
  async createNewProfile(launchProfile = true) {
    await this.maybeSetupDataStore();

    let profile = await this.#createProfile();
    if (launchProfile) {
      this.launchInstance(profile, "about:newprofile");
    }
    return profile;
  }

  /**
   * Add a profile to the profile group datastore.
   *
   * This function assumes the service is initialized and the datastore has
   * been created.
   *
   * @param {object} profileData A plain object that contains a name, avatar,
   *                 themeId, themeFg, themeBg, and relative path as string.
   *
   * @returns {SelectableProfile} The newly created profile object.
   */
  async insertProfile(profileData) {
    // Verify all fields are present.
    let keys = ["avatar", "name", "path", "themeBg", "themeFg", "themeId"];
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
      `INSERT INTO Profiles VALUES (NULL, :path, :name, :avatar, :themeId, :themeFg, :themeBg);`,
      profileData
    );

    this.#notifyTask.arm();

    return this.getProfileByName(profileData.name);
  }

  async deleteProfile(aProfile) {
    if (aProfile.id == this.currentProfile.id) {
      throw new Error(
        "Use `deleteCurrentProfile` to delete the current profile."
      );
    }

    // First attempt to remove the profile's directories. This will attempt to
    // local the directories and so will throw an exception if the profile is
    // currently in use.
    await this.#profileService.removeProfileFilesByPath(
      await aProfile.rootDir,
      null,
      0
    );

    // Then we can remove from the database.
    await this.#connection.execute("DELETE FROM Profiles WHERE id = :id;", {
      id: aProfile.id,
    });

    this.#notifyTask.arm();
  }

  /**
   * Close all active instances running the current profile
   */
  closeActiveProfileInstances() {}

  /**
   * Schedule deletion of the current SelectableProfile as a background task.
   */
  async deleteCurrentProfile() {
    let profiles = await this.getAllProfiles();

    // Refuse to delete the last profile.
    if (profiles.length <= 1) {
      return;
    }

    // TODO: (Bug 1923980) How should we choose the new default profile?
    let newDefault = profiles.find(p => p.id !== this.currentProfile.id);
    await this.setDefaultProfileForGroup(newDefault);

    await this.#connection.executeBeforeShutdown(
      "SelectableProfileService: deleteCurrentProfile",
      db =>
        db.execute("DELETE FROM Profiles WHERE id = :id;", {
          id: this.currentProfile.id,
        })
    );

    if (AppConstants.MOZ_BACKGROUNDTASKS) {
      // Schedule deletion of the profile directories.
      const runner = Cc["@mozilla.org/backgroundtasksrunner;1"].getService(
        Ci.nsIBackgroundTasksRunner
      );
      let rootDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
      let localDir = Services.dirsvc.get("ProfLD", Ci.nsIFile);
      runner.runInDetachedProcess("removeProfileFiles", [
        rootDir.path,
        localDir.path,
        180,
      ]);
    }
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
       SET path = :path, name = :name, avatar = :avatar, themeId = :themeId, themeFg = :themeFg, themeBg = :themeBg
       WHERE id = :id;`,
      profileObj
    );

    if (aSelectableProfile.id == this.#currentProfile.id) {
      // Force a rebuild of the taskbar icon.
      this.#badge = null;
      this.#currentProfile = aSelectableProfile;
    }

    this.#notifyTask.arm();
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

    return (await this.#connection.executeCached("SELECT * FROM Profiles;"))
      .map(row => {
        return new SelectableProfile(row, this);
      })
      .sort((p1, p2) => p1.name.localeCompare(p2.name));
  }

  /**
   * Get the number of profiles in the group.
   *
   * @returns {number}
   *   The number of profiles in the group.
   */
  async getProfileCount() {
    if (!this.#connection) {
      return 0;
    }

    let rows = await this.#connection.executeCached(
      'SELECT COUNT(*) AS "count" FROM "Profiles";'
    );

    return rows[0]?.getResultByName("count") ?? 0;
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
      await this.#connection.executeCached(
        "SELECT * FROM Profiles WHERE id = :id;",
        {
          id: aProfileID,
        }
      )
    )[0];

    return row ? new SelectableProfile(row, this) : null;
  }

  /**
   * Get a specific profile by its name.
   *
   * @param {string} aProfileName The name of the profile
   * @returns {SelectableProfile}
   *   The specific profile.
   */
  async getProfileByName(aProfileName) {
    if (!this.#connection) {
      return null;
    }

    let row = (
      await this.#connection.execute(
        "SELECT * FROM Profiles WHERE name = :name;",
        {
          name: aProfileName,
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
  async getAllDBPrefs() {
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
   * Get the value of a specific shared pref from the database.
   *
   * @param {string} aPrefName The name of the pref to get
   *
   * @returns {any} Value of the pref
   */
  async getDBPref(aPrefName) {
    let rows = await this.#connection.execute(
      "SELECT value, isBoolean FROM SharedPrefs WHERE name = :name;",
      {
        name: aPrefName,
      }
    );

    if (!rows.length) {
      throw new Error(`Unknown preference '${aPrefName}'`);
    }

    return this.getPrefValueFromRow(rows[0]);
  }

  /**
   * Insert or update a pref value in the database, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {any} aPrefValue The value of the pref
   */
  async #setDBPref(aPrefName, aPrefValue) {
    await this.#connection.execute(
      "INSERT INTO SharedPrefs(id, name, value, isBoolean) VALUES (NULL, :name, :value, :isBoolean) ON CONFLICT(name) DO UPDATE SET value=excluded.value, isBoolean=excluded.isBoolean;",
      {
        name: aPrefName,
        value: aPrefValue,
        isBoolean: typeof aPrefValue === "boolean",
      }
    );

    this.#notifyTask.arm();
  }

  // Starts tracking a new shared pref across the profiles.
  async trackPref(aPrefName) {
    await this.flushSharedPrefToDatabase(aPrefName);
  }

  /**
   * Remove a shared pref from the database, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref to delete
   */
  async #deleteDBPref(aPrefName) {
    // We mark the value as null if it already exists in the database so other profiles know what
    // preference to remove.
    await this.#connection.executeCached(
      "UPDATE SharedPrefs SET value=NULL, isBoolean=FALSE WHERE name=:name;",
      {
        name: aPrefName,
      }
    );

    this.#notifyTask.arm();
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

/**
 * A command line handler for receiving notifications from other instances that
 * the profiles database has been updated.
 */
export class CommandLineHandler {
  static classID = Components.ID("{38971986-c834-4f52-bf17-5123fbc9dde5}");
  static contractID = "@mozilla.org/browser/selectable-profiles-service-clh;1";

  QueryInterface = ChromeUtils.generateQI([Ci.nsICommandLineHandler]);

  handle(cmdLine) {
    // This is only ever sent when the application is already running.
    if (cmdLine.handleFlag(COMMAND_LINE_UPDATE, true)) {
      if (SelectableProfileService.initialized) {
        SelectableProfileService.databaseChanged("remote").catch(console.error);
      }
      cmdLine.preventDefault = true;
      return;
    }

    // Sent from the profiles UI to launch a profile if it doesn't exist or bring it to the front
    // if it is already running. In the case where this instance is already running we want to block
    // the normal action of opening a new empty window and instead raise the application to the
    // front manually.
    if (
      cmdLine.handleFlag(COMMAND_LINE_ACTIVATE, true) &&
      cmdLine.state != Ci.nsICommandLine.STATE_INITIAL_LAUNCH
    ) {
      let win = Services.wm.getMostRecentWindow(null);
      if (win) {
        win.focus();
        cmdLine.preventDefault = true;
      }
    }
  }
}
