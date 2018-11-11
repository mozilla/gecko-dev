/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["LaterRun"];

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setInterval", "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "clearInterval", "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "RecentWindow", "resource://gre/modules/RecentWindow.jsm");

const kEnabledPref = "browser.laterrun.enabled";
const kPagePrefRoot = "browser.laterrun.pages.";
// Number of sessions we've been active in
const kSessionCountPref = "browser.laterrun.bookkeeping.sessionCount";
// Time the profile was created at:
const kProfileCreationTime = "browser.laterrun.bookkeeping.profileCreationTime";

// After 50 sessions or 1 month since install, assume we will no longer be
// interested in showing anything to "new" users
const kSelfDestructSessionLimit = 50;
const kSelfDestructHoursLimit = 31 * 24;

class Page {
  constructor({pref, minimumHoursSinceInstall, minimumSessionCount, requireBoth, url}) {
    this.pref = pref;
    this.minimumHoursSinceInstall = minimumHoursSinceInstall || 0;
    this.minimumSessionCount = minimumSessionCount || 1;
    this.requireBoth = requireBoth || false;
    this.url = url;
  }

  get hasRun() {
    return Preferences.get(this.pref + "hasRun", false);
  }

  applies(sessionInfo) {
    if (this.hasRun) {
      return false;
    }
    if (this.requireBoth) {
      return sessionInfo.sessionCount >= this.minimumSessionCount &&
             sessionInfo.hoursSinceInstall >= this.minimumHoursSinceInstall;
    }
    return sessionInfo.sessionCount >= this.minimumSessionCount ||
           sessionInfo.hoursSinceInstall >= this.minimumHoursSinceInstall;
  }
}

let LaterRun = {
  init() {
    if (!this.enabled) {
      return;
    }
    // If this is the first run, set the time we were installed
    if (!Preferences.has(kProfileCreationTime)) {
      // We need to store seconds in order to fit within int prefs.
      Preferences.set(kProfileCreationTime, Math.floor(Date.now() / 1000));
    }
    this.sessionCount++;

    if (this.hoursSinceInstall > kSelfDestructHoursLimit ||
        this.sessionCount > kSelfDestructSessionLimit) {
      this.selfDestruct();
      return;
    }
  },

  // The enabled, hoursSinceInstall and sessionCount properties mirror the
  // preferences system, and are here for convenience.
  get enabled() {
    return Preferences.get(kEnabledPref, false);
  },

  set enabled(val) {
    let wasEnabled = this.enabled;
    Preferences.set(kEnabledPref, val);
    if (val && !wasEnabled) {
      this.init();
    }
  },

  get hoursSinceInstall() {
    let installStamp = Preferences.get(kProfileCreationTime, Date.now() / 1000);
    return Math.floor((Date.now() / 1000 - installStamp) / 3600);
  },

  get sessionCount() {
    if (this._sessionCount) {
      return this._sessionCount;
    }
    return this._sessionCount = Preferences.get(kSessionCountPref, 0);
  },

  set sessionCount(val) {
    this._sessionCount = val;
    Preferences.set(kSessionCountPref, val);
  },

  // Because we don't want to keep incrementing this indefinitely for no reason,
  // we will turn ourselves off after a set amount of time/sessions (see top of
  // file).
  selfDestruct() {
    Preferences.set(kEnabledPref, false);
  },

  // Create an array of Page objects based on the currently set prefs
  readPages() {
    // Enumerate all the pages.
    let allPrefsForPages = Services.prefs.getChildList(kPagePrefRoot);
    let pageDataStore = new Map();
    for (let pref of allPrefsForPages) {
      let [slug, prop] = pref.substring(kPagePrefRoot.length).split(".");
      if (!pageDataStore.has(slug)) {
        pageDataStore.set(slug, {pref: pref.substring(0, pref.length - prop.length)});
      }
      let defaultPrefValue = 0;
      if (prop == "requireBoth" || prop == "hasRun") {
        defaultPrefValue = false;
      } else if (prop == "url") {
        defaultPrefValue = "";
      }
      pageDataStore.get(slug)[prop] = Preferences.get(pref, defaultPrefValue);
    }
    let rv = [];
    for (let [, pageData] of pageDataStore) {
      if (pageData.url) {
        let uri = null;
        try {
          let urlString = Services.urlFormatter.formatURL(pageData.url.trim());
          uri = Services.io.newURI(urlString, null, null);
        } catch (ex) {
          Cu.reportError("Invalid LaterRun page URL " + pageData.url + " ignored.");
          continue;
        }
        if (!uri.schemeIs("https")) {
          Cu.reportError("Insecure LaterRun page URL " + uri.spec + " ignored.");
        } else {
          pageData.url = uri.spec;
          rv.push(new Page(pageData));
        }
      }
    }
    return rv;
  },

  // Return a URL for display as a 'later run' page if its criteria are matched,
  // or null otherwise.
  // NB: will only return one page at a time; if multiple pages match, it's up
  // to the preference service which one gets shown first, and the next one
  // will be shown next startup instead.
  getURL() {
    if (!this.enabled) {
      return null;
    }
    let pages = this.readPages();
    let page = pages.find(page => page.applies(this));
    if (page) {
      Services.prefs.setBoolPref(page.pref + "hasRun", true);
      return page.url;
    }
    return null;
  },
};

LaterRun.init();
