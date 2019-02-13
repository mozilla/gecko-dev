/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "AboutHomeUtils", "AboutHome" ];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");

// Url to fetch snippets, in the urlFormatter service format.
const SNIPPETS_URL_PREF = "browser.aboutHomeSnippets.updateUrl";

// Should be bumped up if the snippets content format changes.
const STARTPAGE_VERSION = 4;

this.AboutHomeUtils = {
  get snippetsVersion() {
    return STARTPAGE_VERSION;
  },

  /*
   * showKnowYourRights - Determines if the user should be shown the
   * about:rights notification. The notification should *not* be shown if
   * we've already shown the current version, or if the override pref says to
   * never show it. The notification *should* be shown if it's never been seen
   * before, if a newer version is available, or if the override pref says to
   * always show it.
   */
  get showKnowYourRights() {
    // Look for an unconditional override pref. If set, do what it says.
    // (true --> never show, false --> always show)
    try {
      return !Services.prefs.getBoolPref("browser.rights.override");
    } catch (e) { }
    // Ditto, for the legacy EULA pref.
    try {
      return !Services.prefs.getBoolPref("browser.EULA.override");
    } catch (e) { }

    if (!AppConstants.MOZILLA_OFFICIAL) {
      // Non-official builds shouldn't show the notification.
      return false;
    }

    // Look to see if the user has seen the current version or not.
    var currentVersion = Services.prefs.getIntPref("browser.rights.version");
    try {
      return !Services.prefs.getBoolPref("browser.rights." + currentVersion + ".shown");
    } catch (e) { }

    // Legacy: If the user accepted a EULA, we won't annoy them with the
    // equivalent about:rights page until the version changes.
    try {
      return !Services.prefs.getBoolPref("browser.EULA." + currentVersion + ".accepted");
    } catch (e) { }

    // We haven't shown the notification before, so do so now.
    return true;
  }
};

/**
 * Returns the URL to fetch snippets from, in the urlFormatter service format.
 */
XPCOMUtils.defineLazyGetter(AboutHomeUtils, "snippetsURL", function() {
  let updateURL = Services.prefs
                          .getCharPref(SNIPPETS_URL_PREF)
                          .replace("%STARTPAGE_VERSION%", STARTPAGE_VERSION);
  return Services.urlFormatter.formatURL(updateURL);
});

/**
 * This code provides services to the about:home page. Whenever
 * about:home needs to do something chrome-privileged, it sends a
 * message that's handled here.
 */
let AboutHome = {
  MESSAGES: [
    "AboutHome:RestorePreviousSession",
    "AboutHome:Downloads",
    "AboutHome:Bookmarks",
    "AboutHome:History",
    "AboutHome:Apps",
    "AboutHome:Addons",
    "AboutHome:Sync",
    "AboutHome:Settings",
    "AboutHome:RequestUpdate",
    "AboutHome:Search",
    "AboutHome:OpenSearchPanel",
  ],

  init: function() {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);

    for (let msg of this.MESSAGES) {
      mm.addMessageListener(msg, this);
    }

    Services.obs.addObserver(this, "browser-search-engine-modified", false);
  },

  observe: function(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case "browser-search-engine-modified":
        this.sendAboutHomeData(null);
        break;
    }
  },

  receiveMessage: function(aMessage) {
    let window = aMessage.target.ownerDocument.defaultView;

    switch (aMessage.name) {
      case "AboutHome:RestorePreviousSession":
        let ss = Cc["@mozilla.org/browser/sessionstore;1"].
                 getService(Ci.nsISessionStore);
        if (ss.canRestoreLastSession) {
          ss.restoreLastSession();
        }
        break;

      case "AboutHome:Downloads":
        window.BrowserDownloadsUI();
        break;

      case "AboutHome:Bookmarks":
        window.PlacesCommandHook.showPlacesOrganizer("AllBookmarks");
        break;

      case "AboutHome:History":
        window.PlacesCommandHook.showPlacesOrganizer("History");
        break;

      case "AboutHome:Apps":
        window.BrowserOpenApps();
        break;

      case "AboutHome:Addons":
        window.BrowserOpenAddonsMgr();
        break;

      case "AboutHome:Sync":
        let weave = Cc["@mozilla.org/weave/service;1"]
                      .getService(Ci.nsISupports)
                      .wrappedJSObject;

        if (weave.fxAccountsEnabled) {
          fxAccounts.getSignedInUser().then(userData => {
            if (userData) {
              window.openPreferences("paneSync");
            } else {
              window.loadURI("about:accounts?entrypoint=abouthome");
            }
          });
        } else {
          window.openPreferences("paneSync");
        }
        break;

      case "AboutHome:Settings":
        window.openPreferences();
        break;

      case "AboutHome:RequestUpdate":
        this.sendAboutHomeData(aMessage.target);
        break;

      case "AboutHome:Search":
        let data;
        try {
          data = JSON.parse(aMessage.data.searchData);
        } catch(ex) {
          Cu.reportError(ex);
          break;
        }

        Services.search.init(function(status) {
          if (!Components.isSuccessCode(status)) {
            return;
          }

          let engine = Services.search.currentEngine;
          if (AppConstants.MOZ_SERVICES_HEALTHREPORT) {
            window.BrowserSearch.recordSearchInHealthReport(engine, "abouthome", data.selection);
          }

          // Trigger a search through nsISearchEngine.getSubmission()
          let submission = engine.getSubmission(data.searchTerms, null, "homepage");
          let where = window.whereToOpenLink(data.originalEvent);

          // There is a chance that by the time we receive the search message, the
          // user has switched away from the tab that triggered the search. If,
          // based on the event, we need to load the search in the same tab that
          // triggered it (i.e. where == "current"), openUILinkIn will not work
          // because that tab is no longer the current one. For this case we
          // manually load the URI in the target browser.
          if (where == "current") {
            aMessage.target.loadURIWithFlags(submission.uri.spec,
                                             Ci.nsIWebNavigation.LOAD_FLAGS_NONE,
                                             null, null, submission.postData);
          } else {
            let params = {
              postData: submission.postData,
              inBackground: Services.prefs.getBoolPref("browser.tabs.loadInBackground"),
            };
            window.openLinkIn(submission.uri.spec, where, params);
          }
          // Used for testing
          let mm = aMessage.target.messageManager;
          mm.sendAsyncMessage("AboutHome:SearchTriggered", aMessage.data.searchData);
        });

        break;

      case "AboutHome:OpenSearchPanel":
        let panel = window.document.getElementById("abouthome-search-panel");
        let anchor = aMessage.objects.anchor;
        panel.hidden = false;
        panel.openPopup(anchor);
        anchor.setAttribute("active", "true");
        panel.addEventListener("popuphidden", function onHidden() {
          panel.removeEventListener("popuphidden", onHidden);
          anchor.removeAttribute("active");
        });
        break;
    }
  },

  // Send all the chrome-privileged data needed by about:home. This
  // gets re-sent when the search engine changes.
  sendAboutHomeData: function(target) {
    let wrapper = {};
    Components.utils.import("resource:///modules/sessionstore/SessionStore.jsm",
      wrapper);
    let ss = wrapper.SessionStore;

    ss.promiseInitialized.then(function() {
      let deferred = Promise.defer();

      Services.search.init(function (status){
        if (!Components.isSuccessCode(status)) {
          deferred.reject(status);
        } else {
          deferred.resolve(Services.search.defaultEngine.name);
        }
      });

      return deferred.promise;
    }).then(function(engineName) {
      let data = {
        showRestoreLastSession: ss.canRestoreLastSession,
        snippetsURL: AboutHomeUtils.snippetsURL,
        showKnowYourRights: AboutHomeUtils.showKnowYourRights,
        snippetsVersion: AboutHomeUtils.snippetsVersion,
        defaultEngineName: engineName
      };

      if (AboutHomeUtils.showKnowYourRights) {
        // Set pref to indicate we've shown the notification.
        let currentVersion = Services.prefs.getIntPref("browser.rights.version");
        Services.prefs.setBoolPref("browser.rights." + currentVersion + ".shown", true);
      }

      if (target && target.messageManager) {
        target.messageManager.sendAsyncMessage("AboutHome:Update", data);
      } else {
        let mm = Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager);
        mm.broadcastAsyncMessage("AboutHome:Update", data);
      }
    }).then(null, function onError(x) {
      Cu.reportError("Error in AboutHome.sendAboutHomeData: " + x);
    });
  },

  /**
   * Focuses the search input in the page with the given message manager.
   * @param  messageManager
   *         The MessageManager object of the selected browser.
   */
  focusInput: function (messageManager) {
    messageManager.sendAsyncMessage("AboutHome:FocusInput");
  }
};
