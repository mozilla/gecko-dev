/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.sys.mjs",
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  DistributionManagement: "resource:///modules/distribution.sys.mjs",
  PlacesBackups: "resource://gre/modules/PlacesBackups.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PlacesUIUtils: "moz-src:///browser/components/places/PlacesUIUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
  UserIdleService: [
    "@mozilla.org/widget/useridleservice;1",
    "nsIUserIdleService",
  ],
});

// Seconds of idle before trying to create a bookmarks backup.
const BOOKMARKS_BACKUP_IDLE_TIME_SEC = 8 * 60;
// Minimum interval between backups.  We try to not create more than one backup
// per interval.
const BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS = 1;

export let PlacesBrowserStartup = {
  _migrationImportsDefaultBookmarks: false,
  _placesInitialized: false,
  _placesBrowserInitComplete: false,
  _isObservingIdle: false,
  _bookmarksBackupIdleTime: null,
  _firstWindowReady: Promise.withResolvers(),

  onFirstWindowReady(window) {
    this._firstWindowReady.resolve();
    // Set the default favicon size for UI views that use the page-icon protocol.
    lazy.PlacesUtils.favicons.setDefaultIconURIPreferredSize(
      16 * window.devicePixelRatio
    );
  },

  backendInitComplete() {
    if (!this._migrationImportsDefaultBookmarks) {
      this.initPlaces();
    }
  },

  willImportDefaultBookmarks() {
    this._migrationImportsDefaultBookmarks = true;
  },

  didImportDefaultBookmarks() {
    this.initPlaces({ initialMigrationPerformed: true });
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks database is empty, try to
   *   restore bookmarks from a JSON backup if the backend indicates that the
   *   database was corrupt.
   *
   * These prefs can be set up by the frontend:
   *
   * WARNING: setting these preferences to true will overwite existing bookmarks
   *
   * - browser.places.importBookmarksHTML
   *   Set to true will import the bookmarks.html file from the profile folder.
   * - browser.bookmarks.restore_default_bookmarks
   *   Set to true by safe-mode dialog to indicate we must restore default
   *   bookmarks.
   *
   * @param {object} [options={}]
   * @param {boolean} [options.initialMigrationPerformed=false]
   *   Whether we performed an initial migration from another browser or via
   *   Firefox Refresh.
   */
  initPlaces({ initialMigrationPerformed = false } = {}) {
    if (this._placesInitialized) {
      throw new Error("Cannot initialize Places more than once");
    }
    this._placesInitialized = true;

    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    let dbStatus = lazy.PlacesUtils.history.databaseStatus;

    // Show a notification with a "more info" link for a locked places.sqlite.
    if (dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_LOCKED) {
      // Note: initPlaces should always happen when the first window is ready,
      // in any case, better safe than sorry.
      this._firstWindowReady.promise.then(() => {
        this._showPlacesLockedNotificationBox();
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
      return;
    }

    let importBookmarks =
      !initialMigrationPerformed &&
      (dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_CREATE ||
        dbStatus == lazy.PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    let importBookmarksHTML = false;
    try {
      importBookmarksHTML = Services.prefs.getBoolPref(
        "browser.places.importBookmarksHTML"
      );
      if (importBookmarksHTML) {
        importBookmarks = true;
      }
    } catch (ex) {}

    // Support legacy bookmarks.html format for apps that depend on that format.
    let autoExportHTML = Services.prefs.getBoolPref(
      "browser.bookmarks.autoExportHTML",
      false
    ); // Do not export.
    if (autoExportHTML) {
      // Sqlite.sys.mjs and Places shutdown happen at profile-before-change, thus,
      // to be on the safe side, this should run earlier.
      lazy.AsyncShutdown.profileChangeTeardown.addBlocker(
        "Places: export bookmarks.html",
        () =>
          lazy.BookmarkHTMLUtils.exportToFile(
            lazy.BookmarkHTMLUtils.defaultPath
          )
      );
    }

    (async () => {
      // Check if Safe Mode or the user has required to restore bookmarks from
      // default profile's bookmarks.html
      let restoreDefaultBookmarks = false;
      try {
        restoreDefaultBookmarks = Services.prefs.getBoolPref(
          "browser.bookmarks.restore_default_bookmarks"
        );
        if (restoreDefaultBookmarks) {
          // Ensure that we already have a bookmarks backup for today.
          await this._backupBookmarks();
          importBookmarks = true;
        }
      } catch (ex) {}

      // If the user did not require to restore default bookmarks, or import
      // from bookmarks.html, we will try to restore from JSON
      if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
        // get latest JSON backup
        let lastBackupFile = await lazy.PlacesBackups.getMostRecentBackup();
        if (lastBackupFile) {
          // restore from JSON backup
          await lazy.BookmarkJSONUtils.importFromFile(lastBackupFile, {
            replace: true,
            source: lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
          });
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (await IOUtils.exists(lazy.BookmarkHTMLUtils.defaultPath)) {
            // If bookmarks.html is available in current profile import it...
            importBookmarksHTML = true;
          } else {
            // ...otherwise we will restore defaults
            restoreDefaultBookmarks = true;
          }
        }
      }

      // Import default bookmarks when necessary.
      // Otherwise, if any kind of import runs, default bookmarks creation should be
      // delayed till the import operations has finished.  Not doing so would
      // cause them to be overwritten by the newly imported bookmarks.
      if (!importBookmarks) {
        // Now apply distribution customized bookmarks.
        // This should always run after Places initialization.
        try {
          await lazy.DistributionManagement.applyBookmarks();
        } catch (e) {
          console.error(e);
        }
      } else {
        // An import operation is about to run.
        let bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore the default set of bookmarks shipped with the
          // browser, those that new profiles start with.
          bookmarksUrl = "chrome://browser/content/default-bookmarks.html";
        } else if (await IOUtils.exists(lazy.BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = PathUtils.toFileURI(
            lazy.BookmarkHTMLUtils.defaultPath
          );
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            if (
              Services.policies.isAllowed("defaultBookmarks") &&
              // Default bookmarks are imported after startup, and they may
              // influence the outcome of tests, thus it's possible to use
              // this test-only pref to skip the import.
              !(
                Cu.isInAutomation &&
                Services.prefs.getBoolPref(
                  "browser.bookmarks.testing.skipDefaultBookmarksImport",
                  false
                )
              )
            ) {
              await lazy.BookmarkHTMLUtils.importFromURL(bookmarksUrl, {
                replace: true,
                source: lazy.PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
              });
            }
          } catch (e) {
            console.error("Bookmarks.html file could be corrupt. ", e);
          }
          try {
            // Now apply distribution customized bookmarks.
            // This should always run after Places initialization.
            await lazy.DistributionManagement.applyBookmarks();
          } catch (e) {
            console.error(e);
          }
        } else {
          console.error(new Error("Unable to find bookmarks.html file."));
        }

        // Reset preferences, so we won't try to import again at next run
        if (importBookmarksHTML) {
          Services.prefs.setBoolPref(
            "browser.places.importBookmarksHTML",
            false
          );
        }
        if (restoreDefaultBookmarks) {
          Services.prefs.setBoolPref(
            "browser.bookmarks.restore_default_bookmarks",
            false
          );
        }
      }

      // Initialize bookmark archiving on idle.
      // If the last backup has been created before the last browser session,
      // and is days old, be more aggressive with the idle timer.
      let idleTime = BOOKMARKS_BACKUP_IDLE_TIME_SEC;
      if (!(await lazy.PlacesBackups.hasRecentBackup())) {
        idleTime /= 2;
      }

      if (!this._isObservingIdle) {
        lazy.UserIdleService.addIdleObserver(this._backupBookmarks, idleTime);
        Services.obs.addObserver(this, "profile-before-change");
        this._isObservingIdle = true;
      }

      this._bookmarksBackupIdleTime = idleTime;

      if (this._isNewProfile) {
        // New profiles may have existing bookmarks (imported from another browser or
        // copied into the profile) and we want to show the bookmark toolbar for them
        // in some cases.
        await lazy.PlacesUIUtils.maybeToggleBookmarkToolbarVisibility();
      }
    })()
      .catch(ex => {
        console.error(ex);
      })
      .then(() => {
        // NB: deliberately after the catch so that we always do this, even if
        // we threw halfway through initializing in the Task above.
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
  },

  /**
   * If a backup for today doesn't exist, this creates one.
   */
  async _backupBookmarks() {
    let lastBackupFile = await lazy.PlacesBackups.getMostRecentBackup();
    // Should backup bookmarks if there are no backups or the maximum
    // interval between backups elapsed.
    if (
      !lastBackupFile ||
      new Date() - lazy.PlacesBackups.getDateForFile(lastBackupFile) >
        BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS * 86400000
    ) {
      let maxBackups = Services.prefs.getIntPref(
        "browser.bookmarks.max_backups"
      );
      await lazy.PlacesBackups.create(maxBackups);
    }
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  async _showPlacesLockedNotificationBox() {
    var win = lazy.BrowserWindowTracker.getTopWindow();
    var buttons = [{ supportPage: "places-locked" }];

    var notifyBox = win.gBrowser.getNotificationBox();
    var notification = await notifyBox.appendNotification(
      "places-locked",
      {
        label: { "l10n-id": "places-locked-prompt" },
        priority: win.gNotificationBox.PRIORITY_CRITICAL_MEDIUM,
      },
      buttons
    );
    notification.persistence = -1; // Until user closes it
  },

  notifyIfInitializationComplete() {
    if (this._placesBrowserInitComplete) {
      Services.obs.notifyObservers(null, "places-browser-init-complete");
    }
  },

  async maybeAddImportButton() {
    // First check if we've already added the import button, in which
    // case we should check for events indicating we can remove it.
    if (
      Services.prefs.getBoolPref("browser.bookmarks.addedImportButton", false)
    ) {
      lazy.PlacesUIUtils.removeImportButtonWhenImportSucceeds();
      return;
    }

    // Otherwise, check if this is a new profile where we need to add it.
    // `maybeAddImportButton` will call
    // `removeImportButtonWhenImportSucceeds`itself if/when it adds the
    // button. Doing things in this order avoids listening for removal
    // more than once.
    if (
      lazy.BrowserHandler.firstRunProfile &&
      // Not in automation: the button changes CUI state, breaking tests
      !Cu.isInAutomation
    ) {
      await lazy.PlacesUIUtils.maybeAddImportButton();
    }
  },

  handleShutdown() {
    if (this._bookmarksBackupIdleTime) {
      lazy.UserIdleService.removeIdleObserver(
        this._backupBookmarks,
        this._bookmarksBackupIdleTime
      );
      this._bookmarksBackupIdleTime = null;
    }
  },

  observe(subject, topic, _data) {
    switch (topic) {
      case "profile-before-change":
        this.handleShutdown();
        break;
    }
  },
};

PlacesBrowserStartup._backupBookmarks =
  PlacesBrowserStartup._backupBookmarks.bind(PlacesBrowserStartup);

PlacesBrowserStartup.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);
