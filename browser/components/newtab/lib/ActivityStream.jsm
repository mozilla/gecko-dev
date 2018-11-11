/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");

// NB: Eagerly load modules that will be loaded/constructed/initialized in the
// common case to avoid the overhead of wrapping and detecting lazy loading.
const {actionCreators: ac, actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {AboutPreferences} = ChromeUtils.import("resource://activity-stream/lib/AboutPreferences.jsm", {});
const {DefaultPrefs} = ChromeUtils.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {ManualMigration} = ChromeUtils.import("resource://activity-stream/lib/ManualMigration.jsm", {});
const {NewTabInit} = ChromeUtils.import("resource://activity-stream/lib/NewTabInit.jsm", {});
const {SectionsFeed} = ChromeUtils.import("resource://activity-stream/lib/SectionsManager.jsm", {});
const {PlacesFeed} = ChromeUtils.import("resource://activity-stream/lib/PlacesFeed.jsm", {});
const {PrefsFeed} = ChromeUtils.import("resource://activity-stream/lib/PrefsFeed.jsm", {});
const {Store} = ChromeUtils.import("resource://activity-stream/lib/Store.jsm", {});
const {SnippetsFeed} = ChromeUtils.import("resource://activity-stream/lib/SnippetsFeed.jsm", {});
const {SystemTickFeed} = ChromeUtils.import("resource://activity-stream/lib/SystemTickFeed.jsm", {});
const {TelemetryFeed} = ChromeUtils.import("resource://activity-stream/lib/TelemetryFeed.jsm", {});
const {FaviconFeed} = ChromeUtils.import("resource://activity-stream/lib/FaviconFeed.jsm", {});
const {TopSitesFeed} = ChromeUtils.import("resource://activity-stream/lib/TopSitesFeed.jsm", {});
const {TopStoriesFeed} = ChromeUtils.import("resource://activity-stream/lib/TopStoriesFeed.jsm", {});
const {HighlightsFeed} = ChromeUtils.import("resource://activity-stream/lib/HighlightsFeed.jsm", {});
const {ASRouterFeed} = ChromeUtils.import("resource://activity-stream/lib/ASRouterFeed.jsm", {});

const DEFAULT_SITES = new Map([
  // This first item is the global list fallback for any unexpected geos
  ["", "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.reddit.com/,https://www.amazon.com/,https://twitter.com/"],
  ["US", "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/"],
  ["CA", "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://www.amazon.ca/,https://twitter.com/"],
  ["DE", "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.de/,https://www.ebay.de/,https://www.wikipedia.org/,https://www.reddit.com/"],
  ["PL", "https://www.youtube.com/,https://www.facebook.com/,https://allegro.pl/,https://www.wikipedia.org/,https://www.olx.pl/,https://www.wykop.pl/"],
  ["RU", "https://vk.com/,https://www.youtube.com/,https://ok.ru/,https://www.avito.ru/,https://www.aliexpress.com/,https://www.wikipedia.org/"],
  ["GB", "https://www.youtube.com/,https://www.facebook.com/,https://www.reddit.com/,https://www.amazon.co.uk/,https://www.bbc.co.uk/,https://www.ebay.co.uk/"],
  ["FR", "https://www.youtube.com/,https://www.facebook.com/,https://www.wikipedia.org/,https://www.amazon.fr/,https://www.leboncoin.fr/,https://twitter.com/"],
]);
const GEO_PREF = "browser.search.region";
const SPOCS_GEOS = ["US"];
const IS_NIGHTLY_OR_UNBRANDED_BUILD = ["nightly", "default"].includes(UpdateUtils.getUpdateChannel(true));

const ONE_HOUR_IN_MS = 60 * 60 * 1000;

// Determine if spocs should be shown for a geo/locale
function showSpocs({geo}) {
  return SPOCS_GEOS.includes(geo);
}

// Configure default Activity Stream prefs with a plain `value` or a `getValue`
// that computes a value. A `value_local_dev` is used for development defaults.
const PREFS_CONFIG = new Map([
  ["default.sites", {
    title: "Comma-separated list of default top sites to fill in behind visited sites",
    getValue: ({geo}) => DEFAULT_SITES.get(DEFAULT_SITES.has(geo) ? geo : ""),
  }],
  ["feeds.section.topstories.options", {
    title: "Configuration options for top stories feed",
    // This is a dynamic pref as it depends on the feed being shown or not
    getValue: args => JSON.stringify({
      api_key_pref: "extensions.pocket.oAuthConsumerKey",
      // Use the opposite value as what default value the feed would have used
      hidden: !PREFS_CONFIG.get("feeds.section.topstories").getValue(args),
      provider_icon: "pocket",
      provider_name: "Pocket",
      read_more_endpoint: "https://getpocket.com/explore/trending?src=fx_new_tab",
      stories_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/global-recs?version=3&consumer_key=$apiKey&locale_lang=${args.locale}&feed_variant=${showSpocs(args) ? "default_spocs_on" : "default_spocs_off"}`,
      stories_referrer: "https://getpocket.com/recommendations",
      topics_endpoint: `https://getpocket.cdn.mozilla.net/v3/firefox/trending-topics?version=2&consumer_key=$apiKey&locale_lang=${args.locale}`,
      model_keys: ["nmf_model_animals", "nmf_model_business", "nmf_model_career", "nmf_model_datascience", "nmf_model_design", "nmf_model_education", "nmf_model_entertainment", "nmf_model_environment", "nmf_model_fashion", "nmf_model_finance", "nmf_model_food", "nmf_model_health", "nmf_model_home", "nmf_model_life", "nmf_model_marketing", "nmf_model_politics", "nmf_model_programming", "nmf_model_science", "nmf_model_shopping", "nmf_model_sports", "nmf_model_tech", "nmf_model_travel", "nb_model_animals", "nb_model_books", "nb_model_business", "nb_model_career", "nb_model_datascience", "nb_model_design", "nb_model_economics", "nb_model_education", "nb_model_entertainment", "nb_model_environment", "nb_model_fashion", "nb_model_finance", "nb_model_food", "nb_model_game", "nb_model_health", "nb_model_history", "nb_model_home", "nb_model_life", "nb_model_marketing", "nb_model_military", "nb_model_philosophy", "nb_model_photography", "nb_model_politics", "nb_model_productivity", "nb_model_programming", "nb_model_psychology", "nb_model_science", "nb_model_shopping", "nb_model_society", "nb_model_space", "nb_model_sports", "nb_model_tech", "nb_model_travel", "nb_model_writing"],
      show_spocs: showSpocs(args),
      personalized: true,
      version: IS_NIGHTLY_OR_UNBRANDED_BUILD ? 2 : 1,
    }),
  }],
  ["showSponsored", {
    title: "Show sponsored cards in spoc experiment (show_spocs in topstories.options has to be set to true as well)",
    value: true,
  }],
  ["pocketCta", {
    title: "Pocket cta and button for logged out users.",
    value: JSON.stringify({
      cta_button: "",
      cta_text: "",
      cta_url: "",
      use_cta: false,
    }),
  }],
  ["filterAdult", {
    title: "Remove adult pages from sites, highlights, etc.",
    value: true,
  }],
  ["migrationExpired", {
    title: "Boolean flag that decides whether to show the migration message or not.",
    value: false,
  }],
  ["migrationLastShownDate", {
    title: "Timestamp when migration message was last shown. In seconds.",
    value: 0,
  }],
  ["migrationRemainingDays", {
    title: "Number of days to show the manual migration message",
    value: 4,
  }],
  ["prerender", {
    title: "Use the prerendered version of activity-stream.html. This is set automatically by PrefsFeed.jsm.",
    value: true,
  }],
  ["showSearch", {
    title: "Show the Search bar",
    value: true,
  }],
  ["disableSnippets", {
    title: "Disable snippets on activity stream",
    value: false,
  }],
  ["topSitesRows", {
    title: "Number of rows of Top Sites to display",
    value: 1,
  }],
  ["telemetry", {
    title: "Enable system error and usage data collection",
    value: true,
    value_local_dev: false,
  }],
  ["telemetry.ut.events", {
    title: "Enable Unified Telemetry event data collection",
    value: AppConstants.EARLY_BETA_OR_EARLIER,
    value_local_dev: false,
  }],
  ["telemetry.ping.endpoint", {
    title: "Telemetry server endpoint",
    value: "https://tiles.services.mozilla.com/v4/links/activity-stream",
  }],
  ["section.highlights.includeVisited", {
    title: "Boolean flag that decides whether or not to show visited pages in highlights.",
    value: true,
  }],
  ["section.highlights.includeBookmarks", {
    title: "Boolean flag that decides whether or not to show bookmarks in highlights.",
    value: true,
  }],
  ["section.highlights.includePocket", {
    title: "Boolean flag that decides whether or not to show saved Pocket stories in highlights.",
    value: true,
  }],
  ["section.highlights.includeDownloads", {
    title: "Boolean flag that decides whether or not to show saved recent Downloads in highlights.",
    value: true,
  }],
  ["section.highlights.rows", {
    title: "Number of rows of Highlights to display",
    value: 2,
  }],
  ["section.topstories.rows", {
    title: "Number of rows of Top Stories to display",
    value: 1,
  }],
  ["sectionOrder", {
    title: "The rendering order for the sections",
    value: "topsites,topstories,highlights",
  }],
  ["improvesearch.noDefaultSearchTile", {
    title: "Experiment to remove tiles that are the same as the default search",
    value: true,
  }],
  ["improvesearch.topSiteSearchShortcuts.searchEngines", {
    title: "An ordered, comma-delimited list of search shortcuts that we should try and pin",
    // This pref is dynamic as the shortcuts vary depending on the region
    getValue: ({geo}) => {
      if (!geo) {
        return "";
      }
      const searchShortcuts = [];
      if (geo === "CN") {
        searchShortcuts.push("baidu");
      } else if (["BY", "KZ", "RU", "TR"].includes(geo)) {
        searchShortcuts.push("yandex");
      } else {
        searchShortcuts.push("google");
      }
      if (["AT", "DE", "FR", "GB", "IT", "JP", "US"].includes(geo)) {
        searchShortcuts.push("amazon");
      }
      return searchShortcuts.join(",");
    },
  }],
  ["improvesearch.topSiteSearchShortcuts.havePinned", {
    title: "A comma-delimited list of search shortcuts that have previously been pinned",
    value: "",
  }],
  ["asrouter.devtoolsEnabled", {
    title: "Are the asrouter devtools enabled?",
    value: false,
  }],
  ["asrouter.userprefs.cfr", {
    title: "Does the user allow CFR recommendations?",
    value: true,
  }],
  ["asrouter.messageProviders", {
    title: "Configuration for ASRouter message providers",

    /**
     * Each provider must have a unique id and a type of "local" or "remote".
     * Local providers must specify the name of an ASRouter message provider.
     * Remote providers must specify a `url` and an `updateCycleInMs`.
     * Each provider must also have an `enabled` boolean.
     */
    value: JSON.stringify([{
      id: "onboarding",
      type: "local",
      localProvider: "OnboardingMessageProvider",
      enabled: true,
    }, {
      id: "snippets",
      type: "remote",
      url: "https://snippets.cdn.mozilla.net/%STARTPAGE_VERSION%/%NAME%/%VERSION%/%APPBUILDID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/",
      updateCycleInMs: ONE_HOUR_IN_MS * 4,
      enabled: false,
    }, {
      id: "cfr",
      type: "local",
      localProvider: "CFRMessageProvider",
      enabled: IS_NIGHTLY_OR_UNBRANDED_BUILD,
      cohort: IS_NIGHTLY_OR_UNBRANDED_BUILD ? "nightly" : "",
    }]),
  }],
]);

// Array of each feed's FEEDS_CONFIG factory and values to add to PREFS_CONFIG
const FEEDS_DATA = [
  {
    name: "aboutpreferences",
    factory: () => new AboutPreferences(),
    title: "about:preferences rendering",
    value: true,
  },
  {
    name: "migration",
    factory: () => new ManualMigration(),
    title: "Manual migration wizard",
    value: true,
  },
  {
    name: "newtabinit",
    factory: () => new NewTabInit(),
    title: "Sends a copy of the state to each new tab that is opened",
    value: true,
  },
  {
    name: "places",
    factory: () => new PlacesFeed(),
    title: "Listens for and relays various Places-related events",
    value: true,
  },
  {
    name: "prefs",
    factory: () => new PrefsFeed(PREFS_CONFIG),
    title: "Preferences",
    value: true,
  },
  {
    name: "sections",
    factory: () => new SectionsFeed(),
    title: "Manages sections",
    value: true,
  },
  {
    name: "section.highlights",
    factory: () => new HighlightsFeed(),
    title: "Fetches content recommendations from places db",
    value: true,
  },
  {
    name: "section.topstories",
    factory: () => new TopStoriesFeed(),
    title: "Fetches content recommendations from a configurable content provider",
    // Dynamically determine if Pocket should be shown for a geo / locale
    getValue: ({geo, locale}) => {
      const locales = ({
        "US": ["en-CA", "en-GB", "en-US", "en-ZA"],
        "CA": ["en-CA", "en-GB", "en-US", "en-ZA"],
        "DE": ["de", "de-DE", "de-AT", "de-CH"],
      })[geo];
      return !!locales && locales.includes(locale);
    },
  },
  {
    name: "snippets",
    factory: () => new SnippetsFeed(),
    title: "Gets snippets data",
    value: true,
  },
  {
    name: "systemtick",
    factory: () => new SystemTickFeed(),
    title: "Produces system tick events to periodically check for data expiry",
    value: true,
  },
  {
    name: "telemetry",
    factory: () => new TelemetryFeed(),
    title: "Relays telemetry-related actions to PingCentre",
    value: true,
  },
  {
    name: "favicon",
    factory: () => new FaviconFeed(),
    title: "Fetches tippy top manifests from remote service",
    value: true,
  },
  {
    name: "topsites",
    factory: () => new TopSitesFeed(),
    title: "Queries places and gets metadata for Top Sites section",
    value: true,
  },
  {
    name: "asrouterfeed",
    factory: () => new ASRouterFeed(),
    title: "Handles AS Router messages, such as snippets and onboaridng",
    value: true,
  },
];

const FEEDS_CONFIG = new Map();
for (const config of FEEDS_DATA) {
  const pref = `feeds.${config.name}`;
  FEEDS_CONFIG.set(pref, config.factory);
  PREFS_CONFIG.set(pref, config);
}

this.ActivityStream = class ActivityStream {
  /**
   * constructor - Initializes an instance of ActivityStream
   */
  constructor() {
    this.initialized = false;
    this.store = new Store();
    this.feeds = FEEDS_CONFIG;
    this._defaultPrefs = new DefaultPrefs(PREFS_CONFIG);
  }

  init() {
    try {
      this._migratePrefs();
      this._updateDynamicPrefs();
      this._defaultPrefs.init();

      // Hook up the store and let all feeds and pages initialize
      this.store.init(this.feeds, ac.BroadcastToContent({
        type: at.INIT,
        data: {},
      }), {type: at.UNINIT});

      this.initialized = true;
    } catch (e) {
      // TelemetryFeed could be unavailable if the telemetry is disabled, or
      // the telemetry feed is not yet initialized.
      const telemetryFeed = this.store.feeds.get("feeds.telemetry");
      if (telemetryFeed) {
        telemetryFeed.handleUndesiredEvent({data: {event: "ADDON_INIT_FAILED"}});
      }
      throw e;
    }
  }

  /**
   * Check if an old pref has a custom value to migrate. Clears the pref so that
   * it's the default after migrating (to avoid future need to migrate).
   *
   * @param oldPrefName {string} Pref to check and migrate
   * @param cbIfNotDefault {function} Callback that gets the current pref value
   */
  _migratePref(oldPrefName, cbIfNotDefault) {
    // Nothing to do if the user doesn't have a custom value
    if (!Services.prefs.prefHasUserValue(oldPrefName)) {
      return;
    }

    // Figure out what kind of pref getter to use
    let prefGetter;
    switch (Services.prefs.getPrefType(oldPrefName)) {
      case Services.prefs.PREF_BOOL:
        prefGetter = "getBoolPref";
        break;
      case Services.prefs.PREF_INT:
        prefGetter = "getIntPref";
        break;
      case Services.prefs.PREF_STRING:
        prefGetter = "getStringPref";
        break;
    }

    // Give the callback the current value then clear the pref
    cbIfNotDefault(Services.prefs[prefGetter](oldPrefName));
    Services.prefs.clearUserPref(oldPrefName);
  }

  _migratePrefs() {
    // Do a one time migration of Tiles about:newtab prefs that have been modified
    this._migratePref("browser.newtabpage.rows", rows => {
      // Just disable top sites if rows are not desired
      if (rows <= 0) {
        Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.topsites", false);
      } else {
        Services.prefs.setIntPref("browser.newtabpage.activity-stream.topSitesRows", rows);
      }
    });

    this._migratePref("browser.newtabpage.activity-stream.showTopSites", value => {
      if (value === false) {
        Services.prefs.setBoolPref("browser.newtabpage.activity-stream.feeds.topsites", false);
      }
    });

    // Old activity stream topSitesCount pref showed 6 per row
    this._migratePref("browser.newtabpage.activity-stream.topSitesCount", count => {
      Services.prefs.setIntPref("browser.newtabpage.activity-stream.topSitesRows", Math.ceil(count / 6));
    });
  }

  uninit() {
    if (this.geo === "") {
      Services.prefs.removeObserver(GEO_PREF, this);
    }

    this.store.uninit();
    this.initialized = false;
  }

  _updateDynamicPrefs() {
    // Save the geo pref if we have it
    if (Services.prefs.prefHasUserValue(GEO_PREF)) {
      this.geo = Services.prefs.getStringPref(GEO_PREF);
    } else if (this.geo !== "") {
      // Watch for geo changes and use a dummy value for now
      Services.prefs.addObserver(GEO_PREF, this);
      this.geo = "";
    }

    this.locale = Services.locale.appLocaleAsLangTag;

    // Update the pref config of those with dynamic values
    for (const pref of PREFS_CONFIG.keys()) {
      const prefConfig = PREFS_CONFIG.get(pref);
      if (!prefConfig.getValue) {
        continue;
      }

      const newValue = prefConfig.getValue({
        geo: this.geo,
        locale: this.locale,
      });

      // If there's an existing value and it has changed, that means we need to
      // overwrite the default with the new value.
      if (prefConfig.value !== undefined && prefConfig.value !== newValue) {
        this._defaultPrefs.set(pref, newValue);
      }

      prefConfig.value = newValue;
    }
  }

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        // We should only expect one geo change, so update and stop observing
        if (data === GEO_PREF) {
          this._updateDynamicPrefs();
          Services.prefs.removeObserver(GEO_PREF, this);
        }
        break;
    }
  }
};

const EXPORTED_SYMBOLS = ["ActivityStream", "PREFS_CONFIG"];
