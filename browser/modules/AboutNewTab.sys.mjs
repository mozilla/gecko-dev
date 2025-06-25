/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActivityStream: "resource://newtab/lib/ActivityStream.sys.mjs",
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
});

const ABOUT_URL = "about:newtab";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";
// AboutHomeStartupCache needs us in "quit-application", so stay alive longer.
// TODO: We could better have a shared async shutdown blocker?
const TOPIC_APP_QUIT = "profile-before-change";
const BROWSER_READY_NOTIFICATION = "sessionstore-windows-restored";
const BUILTIN_ADDON_ID = "newtab@mozilla.org";

export const AboutNewTab = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  // AboutNewTab
  initialized: false,

  willNotifyUser: false,

  _activityStreamEnabled: false,
  activityStream: null,
  activityStreamDebug: false,

  _cachedTopSites: null,

  _newTabURL: ABOUT_URL,
  _newTabURLOverridden: false,

  /**
   * init - Initializes an instance of Activity Stream if one doesn't exist already.
   */
  init() {
    Services.obs.addObserver(this, TOPIC_APP_QUIT);
    if (!AppConstants.RELEASE_OR_BETA) {
      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "activityStreamDebug",
        PREF_ACTIVITY_STREAM_DEBUG,
        false,
        () => {
          this.notifyChange();
        }
      );
    }

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "privilegedAboutProcessEnabled",
      "browser.tabs.remote.separatePrivilegedContentProcess",
      false,
      () => {
        this.notifyChange();
      }
    );

    // More initialization happens here
    this.toggleActivityStream(true);
    this.initialized = true;

    Services.obs.addObserver(this, BROWSER_READY_NOTIFICATION);
  },

  async uninstallAddon() {
    let addon = await lazy.AddonManager.getAddonByID(BUILTIN_ADDON_ID);
    if (addon) {
      addon.uninstall();
    }
  },

  /**
   * React to changes to the activity stream being enabled or not.
   *
   * This will only act if there is a change of state and if not overridden.
   *
   * @returns {Boolean} Returns if there has been a state change
   *
   * @param {Boolean}   stateEnabled    activity stream enabled state to set to
   * @param {Boolean}   forceState      force state change
   */
  toggleActivityStream(stateEnabled, forceState = false) {
    if (
      !forceState &&
      (this._newTabURLOverridden ||
        stateEnabled === this._activityStreamEnabled)
    ) {
      // exit there is no change of state
      return false;
    }
    if (stateEnabled) {
      this._activityStreamEnabled = true;
    } else {
      this._activityStreamEnabled = false;
    }

    this._newTabURL = ABOUT_URL;
    return true;
  },

  get newTabURL() {
    return this._newTabURL;
  },

  set newTabURL(aNewTabURL) {
    let newTabURL = aNewTabURL.trim();
    if (newTabURL === ABOUT_URL) {
      // avoid infinite redirects in case one sets the URL to about:newtab
      this.resetNewTabURL();
      return;
    } else if (newTabURL === "") {
      newTabURL = "about:blank";
    }

    this.toggleActivityStream(false);
    this._newTabURL = newTabURL;
    this._newTabURLOverridden = true;
    this.notifyChange();
  },

  get newTabURLOverridden() {
    return this._newTabURLOverridden;
  },

  get activityStreamEnabled() {
    return this._activityStreamEnabled;
  },

  resetNewTabURL() {
    this._newTabURLOverridden = false;
    this._newTabURL = ABOUT_URL;
    this.toggleActivityStream(true, true);
    this.notifyChange();
  },

  notifyChange() {
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
  },

  /**
   * onBrowserReady - Continues the initialization of Activity Stream after browser is ready.
   */
  async onBrowserReady() {
    if (this.activityStream && this.activityStream.initialized) {
      return;
    }

    if (AppConstants.BROWSER_NEWTAB_AS_ADDON) {
      // Wait until the built-in addon has reported that it has finished
      // initializing.
      let redirector = Cc[
        "@mozilla.org/network/protocol/about;1?what=newtab"
      ].getService(Ci.nsIAboutModule).wrappedJSObject;

      await redirector.promiseBuiltInAddonInitialized;
    } else {
      // We may have had the built-in addon installed in the past. Since the
      // flag is false, let's go ahead and remove it. We don't need to await on
      // this since the extension should be inert if the build flag is false.
      this.uninstallAddon();
    }

    try {
      this.activityStream = new lazy.ActivityStream();
      Glean.newtab.activityStreamCtorSuccess.set(true);
    } catch (error) {
      // Send Activity Stream loading failure telemetry
      // This probe will help to monitor if ActivityStream failure has crossed
      // a threshold and send alert. See Bug 1965278
      Glean.newtab.activityStreamCtorSuccess.set(false);
      console.error(error);
      throw error;
    }

    try {
      this.activityStream.init();
      this._subscribeToActivityStream();
    } catch (e) {
      console.error(e);
    }
  },

  _subscribeToActivityStream() {
    let unsubscribe = this.activityStream.store.subscribe(() => {
      // If the top sites changed, broadcast "newtab-top-sites-changed". We
      // ignore changes to the `screenshot` property in each site because
      // screenshots are generated at times that are hard to predict and it ends
      // up interfering with tests that rely on "newtab-top-sites-changed".
      // Observers likely don't care about screenshots anyway.
      let topSites = this.activityStream.store
        .getState()
        .TopSites.rows.map(site => {
          site = { ...site };
          delete site.screenshot;
          return site;
        });
      if (!lazy.ObjectUtils.deepEqual(topSites, this._cachedTopSites)) {
        this._cachedTopSites = topSites;
        Services.obs.notifyObservers(null, "newtab-top-sites-changed");
      }
    });
    this._unsubscribeFromActivityStream = () => {
      try {
        unsubscribe();
      } catch (e) {
        console.error(e);
      }
    };
  },

  /**
   * uninit - Uninitializes Activity Stream if it exists.
   */
  uninit() {
    if (this.activityStream) {
      this._unsubscribeFromActivityStream?.();
      this.activityStream.uninit();
      this.activityStream = null;
    }

    this.initialized = false;
  },

  getTopSites() {
    return this.activityStream
      ? this.activityStream.store.getState().TopSites.rows
      : [];
  },

  _alreadyRecordedTopsitesPainted: false,
  _nonDefaultStartup: false,

  noteNonDefaultStartup() {
    this._nonDefaultStartup = true;
  },

  maybeRecordTopsitesPainted(timestamp) {
    if (this._alreadyRecordedTopsitesPainted || this._nonDefaultStartup) {
      return;
    }

    let startupInfo = Services.startup.getStartupInfo();
    let processStartTs = startupInfo.process.getTime();
    let delta = Math.round(timestamp - processStartTs);
    Glean.timestamps.aboutHomeTopsitesFirstPaint.set(delta);
    ChromeUtils.addProfilerMarker("aboutHomeTopsitesFirstPaint");
    this._alreadyRecordedTopsitesPainted = true;
  },

  // nsIObserver implementation

  observe(subject, topic) {
    switch (topic) {
      case TOPIC_APP_QUIT: {
        this.uninit();
        break;
      }
      case BROWSER_READY_NOTIFICATION: {
        Services.obs.removeObserver(this, BROWSER_READY_NOTIFICATION);
        // Avoid running synchronously during this event that's used for timing
        Services.tm.dispatchToMainThread(() => this.onBrowserReady());
        break;
      }
    }
  },
};
