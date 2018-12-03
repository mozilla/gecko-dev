/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/E10SUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AboutNewTab",
                               "resource:///modules/AboutNewTab.jsm");

const TOPIC_APP_QUIT = "quit-application-granted";
const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";
const TOPIC_CONTENT_DOCUMENT_INTERACTIVE = "content-document-interactive";

// Automated tests ensure packaged locales are in this list. Copied output of:
// https://github.com/mozilla/activity-stream/blob/master/bin/render-activity-stream-html.js
const ACTIVITY_STREAM_BCP47 = "en-US ach an ar ast az be bg bn-BD bn-IN br bs ca cak crh cs cy da de dsb el en-CA en-GB eo es-AR es-CL es-ES es-MX et eu fa ff fi fr fy-NL ga-IE gd gl gn gu-IN he hi-IN hr hsb hu hy-AM ia id is it ja ja-JP-macos ka kab kk km kn ko lij lo lt ltg lv mai mk ml mr ms my nb-NO ne-NP nl nn-NO oc pa-IN pl pt-BR pt-PT rm ro ru si sk sl sq sr sv-SE ta te th tl tr uk ur uz vi zh-CN zh-TW".split(" ");

const ABOUT_URL = "about:newtab";
const BASE_URL = "resource://activity-stream/";
const ACTIVITY_STREAM_PAGES = new Set(["home", "newtab", "welcome"]);

const IS_MAIN_PROCESS = Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;
const IS_PRIVILEGED_PROCESS = Services.appinfo.remoteType === E10SUtils.PRIVILEGED_REMOTE_TYPE;

const IS_RELEASE_OR_BETA = AppConstants.RELEASE_OR_BETA;

const PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS = "browser.tabs.remote.separatePrivilegedContentProcess";
const PREF_ACTIVITY_STREAM_PRERENDER_ENABLED = "browser.newtabpage.activity-stream.prerender";
const PREF_ACTIVITY_STREAM_DEBUG = "browser.newtabpage.activity-stream.debug";

function AboutNewTabService() {
  Services.obs.addObserver(this, TOPIC_APP_QUIT);
  Services.obs.addObserver(this, TOPIC_LOCALES_CHANGE);
  Services.prefs.addObserver(PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS, this);
  Services.prefs.addObserver(PREF_ACTIVITY_STREAM_PRERENDER_ENABLED, this);
  if (!IS_RELEASE_OR_BETA) {
    Services.prefs.addObserver(PREF_ACTIVITY_STREAM_DEBUG, this);
  }

  // More initialization happens here
  this.toggleActivityStream(true);
  this.initialized = true;

  if (IS_MAIN_PROCESS) {
    AboutNewTab.init();
  } else if (IS_PRIVILEGED_PROCESS) {
    Services.obs.addObserver(this, TOPIC_CONTENT_DOCUMENT_INTERACTIVE);
  }
}

/*
 * A service that allows for the overriding, at runtime, of the newtab page's url.
 *
 * There is tight coupling with browser/about/AboutRedirector.cpp.
 *
 * 1. Browser chrome access:
 *
 * When the user issues a command to open a new tab page, usually clicking a button
 * in the browser chrome or using shortcut keys, the browser chrome code invokes the
 * service to obtain the newtab URL. It then loads that URL in a new tab.
 *
 * When not overridden, the default URL emitted by the service is "about:newtab".
 * When overridden, it returns the overriden URL.
 *
 * 2. Redirector Access:
 *
 * When the URL loaded is about:newtab, the default behavior, or when entered in the
 * URL bar, the redirector is hit. The service is then called to return the
 * appropriate activity stream url based on prefs and locales.
 *
 * NOTE: "about:newtab" will always result in a default newtab page, and never an overridden URL.
 *
 * Access patterns:
 *
 * The behavior is different when accessing the service via browser chrome or via redirector
 * largely to maintain compatibility with expectations of add-on developers.
 *
 * Loading a chrome resource, or an about: URL in the redirector with either the
 * LOAD_NORMAL or LOAD_REPLACE flags yield unexpected behaviors, so a roundtrip
 * to the redirector from browser chrome is avoided.
 */
AboutNewTabService.prototype = {

  _newTabURL: ABOUT_URL,
  _activityStreamEnabled: false,
  _activityStreamPrerender: false,
  _activityStreamPath: "",
  _activityStreamDebug: false,
  _privilegedContentProcess: false,
  _overridden: false,
  willNotifyUser: false,

  classID: Components.ID("{dfcd2adc-7867-4d3a-ba70-17501f208142}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIAboutNewTabService,
    Ci.nsIObserver,
  ]),

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        if (data === PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS) {
          this._privilegedContentProcess = Services.prefs.getBoolPref(PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS);
          this.updatePrerenderedPath();
          this.notifyChange();
        } else if (data === PREF_ACTIVITY_STREAM_PRERENDER_ENABLED) {
          this._activityStreamPrerender = Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_PRERENDER_ENABLED);
          this.notifyChange();
        } else if (!IS_RELEASE_OR_BETA && data === PREF_ACTIVITY_STREAM_DEBUG) {
          this._activityStreamDebug = Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_DEBUG, false);
          this.updatePrerenderedPath();
          this.notifyChange();
        }
        break;
      case TOPIC_CONTENT_DOCUMENT_INTERACTIVE: {
        const win = subject.defaultView;

        // It seems like "content-document-interactive" is triggered multiple
        // times for a single window. The first event always seems to be an
        // HTMLDocument object that contains a non-null window reference
        // whereas the remaining ones seem to be proxied objects.
        // https://searchfox.org/mozilla-central/rev/d2966246905102b36ef5221b0e3cbccf7ea15a86/devtools/server/actors/object.js#100-102
        if (win === null) {
          break;
        }

        // We use win.location.pathname instead of win.location.toString()
        // because we want to account for URLs that contain the location hash
        // property or query strings (e.g. about:newtab#foo, about:home?bar).
        // Asserting here would be ideal, but this code path is also taken
        // by the view-source:// scheme, so we should probably just bail out
        // and do nothing.
        if (!ACTIVITY_STREAM_PAGES.has(win.location.pathname)) {
          break;
        }

        const onLoaded = () => {
          const debugString = this._activityStreamDebug ? "-dev" : "";

          // This list must match any similar ones in render-activity-stream-html.js.
          const scripts = [
            "chrome://browser/content/contentSearchUI.js",
            "chrome://browser/content/contentTheme.js",
            `${BASE_URL}vendor/react${debugString}.js`,
            `${BASE_URL}vendor/react-dom${debugString}.js`,
            `${BASE_URL}vendor/prop-types.js`,
            `${BASE_URL}vendor/react-intl.js`,
            `${BASE_URL}vendor/redux.js`,
            `${BASE_URL}vendor/react-redux.js`,
            `${BASE_URL}prerendered/${this.activityStreamLocale}/activity-stream-strings.js`,
            `${BASE_URL}data/content/activity-stream.bundle.js`,
          ];

          if (this._activityStreamPrerender) {
            scripts.unshift(`${BASE_URL}prerendered/static/activity-stream-initial-state.js`);
          }

          for (let script of scripts) {
            Services.scriptloader.loadSubScript(script, win, "UTF-8"); // Synchronous call
          }
        };
        subject.addEventListener("DOMContentLoaded", onLoaded, {once: true});

        // There is a possibility that DOMContentLoaded won't be fired. This
        // unload event (which cannot be cancelled) will attempt to remove
        // the listener for the DOMContentLoaded event.
        const onUnloaded = () => {
          subject.removeEventListener("DOMContentLoaded", onLoaded);
        };
        subject.addEventListener("unload", onUnloaded, {once: true});
        break;
      }
      case TOPIC_APP_QUIT:
        this.uninit();
        if (IS_MAIN_PROCESS) {
          AboutNewTab.uninit();
        } else if (IS_PRIVILEGED_PROCESS) {
          Services.obs.removeObserver(this, TOPIC_CONTENT_DOCUMENT_INTERACTIVE);
        }
        break;
      case TOPIC_LOCALES_CHANGE:
        this.updatePrerenderedPath();
        this.notifyChange();
        break;
    }
  },

  notifyChange() {
    Services.obs.notifyObservers(null, "newtab-url-changed", this._newTabURL);
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
    if (!forceState && (this.overridden || stateEnabled === this.activityStreamEnabled)) {
      // exit there is no change of state
      return false;
    }
    if (stateEnabled) {
      this._activityStreamEnabled = true;
    } else {
      this._activityStreamEnabled = false;
    }
    this._privilegedContentProcess = Services.prefs.getBoolPref(PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS);
    this._activityStreamPrerender = Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_PRERENDER_ENABLED);
    if (!IS_RELEASE_OR_BETA) {
      this._activityStreamDebug = Services.prefs.getBoolPref(PREF_ACTIVITY_STREAM_DEBUG, false);
    }
    this.updatePrerenderedPath();
    this._newtabURL = ABOUT_URL;
    return true;
  },

  /**
   * Figure out what path under prerendered to use based on current state.
   */
  updatePrerenderedPath() {
    // Debug files are specially packaged in a non-localized directory, but with
    // dynamic script loading, localized debug is supported.
    this._activityStreamPath = `${this._activityStreamDebug &&
      !this._privilegedContentProcess ? "static" : this.activityStreamLocale}/`;
  },

  /*
   * Returns the default URL.
   *
   * This URL depends on various activity stream prefs and locales. Overriding
   * the newtab page has no effect on the result of this function.
   */
  get defaultURL() {
    // Generate the desired activity stream resource depending on state, e.g.,
    // resource://activity-stream/prerendered/ar/activity-stream.html
    // resource://activity-stream/prerendered/en-US/activity-stream-prerendered.html
    // resource://activity-stream/prerendered/static/activity-stream-debug.html
    return [
      "resource://activity-stream/prerendered/",
      this._activityStreamPath,
      "activity-stream",
      this._activityStreamPrerender ? "-prerendered" : "",
      // Debug version loads dev scripts but noscripts separately loads scripts
      this._activityStreamDebug && !this._privilegedContentProcess ? "-debug" : "",
      this._privilegedContentProcess ? "-noscripts" : "",
      ".html",
    ].join("");
  },

  /*
   * Returns the about:welcome URL
   *
   * This is calculated in the same way the default URL is, except that we don't
   * allow prerendering.
   */
  get welcomeURL() {
    const prerenderEnabled = this._activityStreamPrerender;
    this._activityStreamPrerender = false;
    const url = this.defaultURL;
    this._activityStreamPrerender = prerenderEnabled;
    return url;
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
    this._overridden = true;
    this.notifyChange();
  },

  get overridden() {
    return this._overridden;
  },

  get activityStreamEnabled() {
    return this._activityStreamEnabled;
  },

  get activityStreamPrerender() {
    return this._activityStreamPrerender;
  },

  get activityStreamDebug() {
    return this._activityStreamDebug;
  },

  get activityStreamLocale() {
    // Pick the best available locale to match the app locales
    return Services.locale.negotiateLanguages(
      // Fix up incorrect BCP47 that are actually lang tags as a workaround for
      // bug 1479606 returning the wrong values in the content process
      Services.locale.appLocalesAsBCP47.map(l => l.replace(/^(ja-JP-mac)$/, "$1os")),
      ACTIVITY_STREAM_BCP47,
      // defaultLocale's strings aren't necessarily packaged, but en-US' are
      "en-US",
      Services.locale.langNegStrategyLookup
    // Convert the BCP47 to lang tag, which is what is used in our paths, as a
    // workaround for bug 1478930 negotiating incorrectly with lang tags
    )[0].replace(/^(ja-JP-mac)os$/, "$1");
  },

  resetNewTabURL() {
    this._overridden = false;
    this._newTabURL = ABOUT_URL;
    this.toggleActivityStream(true, true);
    this.notifyChange();
  },

  uninit() {
    if (!this.initialized) {
      return;
    }
    Services.obs.removeObserver(this, TOPIC_APP_QUIT);
    Services.obs.removeObserver(this, TOPIC_LOCALES_CHANGE);
    Services.prefs.removeObserver(PREF_SEPARATE_PRIVILEGED_CONTENT_PROCESS, this);
    Services.prefs.removeObserver(PREF_ACTIVITY_STREAM_PRERENDER_ENABLED, this);
    if (!IS_RELEASE_OR_BETA) {
      Services.prefs.removeObserver(PREF_ACTIVITY_STREAM_DEBUG, this);
    }
    this.initialized = false;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AboutNewTabService]);
