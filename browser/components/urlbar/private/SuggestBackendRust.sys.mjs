/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BaseFeature } from "resource:///modules/urlbar/private/BaseFeature.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  InterruptKind: "resource://gre/modules/RustSuggest.sys.mjs",
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  RemoteSettingsServer: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestIngestionConstraints: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestStoreBuilder: "resource://gre/modules/RustSuggest.sys.mjs",
  Suggestion: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionProvider: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionProviderConstraints: "resource://gre/modules/RustSuggest.sys.mjs",
  SuggestionQuery: "resource://gre/modules/RustSuggest.sys.mjs",
  TaskQueue: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  Utils: "resource://services-settings/Utils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const SUGGEST_DATA_STORE_BASENAME = "suggest.sqlite";

// This ID is used to register our ingest timer with nsIUpdateTimerManager.
const INGEST_TIMER_ID = "suggest-ingest";
const INGEST_TIMER_LAST_UPDATE_PREF = `app.update.lastUpdateTime.${INGEST_TIMER_ID}`;

// Maps from `suggestion.constructor` to the corresponding name of the
// suggestion type. See `getSuggestionType()` for details.
const gSuggestionTypesByCtor = new WeakMap();

/**
 * The Suggest Rust backend. Not used when the remote settings JS backend is
 * enabled.
 *
 * This class returns suggestions served by the Rust component. These are the
 * primary related architectural pieces (see bug 1851256 for details):
 *
 * (1) The `suggest` Rust component, which lives in the application-services
 *     repo [1] and is periodically vendored into mozilla-central [2] and then
 *     built into the Firefox binary.
 * (2) `suggest.udl`, which is part of the Rust component's source files and
 *     defines the interface exposed to foreign-function callers like JS [3, 4].
 * (3) `RustSuggest.sys.mjs` [5], which contains the JS bindings generated from
 *     `suggest.udl` by UniFFI. The classes defined in `RustSuggest.sys.mjs` are
 *     what we consume here in this file. If you have a question about the JS
 *     interface to the Rust component, try checking `RustSuggest.sys.mjs`, but
 *     as you get accustomed to UniFFI JS conventions you may find it simpler to
 *     refer directly to `suggest.udl`.
 * (4) `config.toml` [6], which defines which functions in the JS bindings are
 *     sync and which are async. Functions default to the "worker" thread, which
 *     means they are async. Some functions are "main", which means they are
 *     sync. Async functions return promises. This information is reflected in
 *     `RustSuggest.sys.mjs` of course: If a function is "worker", its JS
 *     binding will return a promise, and if it's "main" it won't.
 *
 * [1] https://github.com/mozilla/application-services/tree/main/components/suggest
 * [2] https://searchfox.org/mozilla-central/source/third_party/rust/suggest
 * [3] https://github.com/mozilla/application-services/blob/main/components/suggest/src/suggest.udl
 * [4] https://searchfox.org/mozilla-central/source/third_party/rust/suggest/src/suggest.udl
 * [5] https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js/components/generated/RustSuggest.sys.mjs
 * [6] https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js/config.toml
 */
export class SuggestBackendRust extends BaseFeature {
  constructor(...args) {
    super(...args);
    this.#ingestQueue = new lazy.TaskQueue();
    this.#setRemoteSettingsConfig({
      serverUrl: lazy.Utils.SERVER_URL,
      bucketName: lazy.Utils.actualBucketName("main"),
    });
  }

  /**
   * @returns {object}
   *   The global Suggest config from the Rust component as returned from
   *   `SuggestStore.fetchGlobalConfig()`.
   */
  get config() {
    return this.#config || {};
  }

  /**
   * @returns {Promise}
   *   Resolved when all pending ingests are done.
   */
  get ingestPromise() {
    return this.#ingestQueue.emptyPromise;
  }

  get shouldEnable() {
    return lazy.UrlbarPrefs.get("quickSuggestRustEnabled");
  }

  enable(enabled) {
    if (enabled) {
      this.#init();
    } else {
      this.#uninit();
    }
  }

  /**
   * Queries the Rust component and returns all matching suggestions.
   *
   * @param {string} searchString
   *   The search string.
   * @param {Array} types
   *   This is only intended to be used in special circumstances and normally
   *   should not be specified. Array of suggestion types to query. By default
   *   all enabled suggestion types are queried.
   * @returns {Array}
   *   Matching Rust suggestions.
   */
  async query(searchString, types = null) {
    if (!this.#store) {
      return [];
    }

    this.logger.debug("Handling query", { searchString });

    if (!types) {
      types = this.#enabledSuggestionTypes;
    } else {
      types = types.map(type => {
        let provider = this.#providerFromSuggestionType(type);
        if (!provider) {
          throw new Error("Unknown Rust suggestion type: " + type);
        }
        return { type, provider };
      });
    }

    let providers = [];
    let allProviderConstraints = {};
    for (let { type, provider } of types) {
      this.logger.debug("Adding type to query", { type, provider });
      providers.push(provider);

      let providerConstraints =
        lazy.QuickSuggest.getFeatureByRustSuggestionType(
          type
        ).getRustProviderConstraints(type);
      if (providerConstraints) {
        allProviderConstraints = {
          ...allProviderConstraints,
          ...providerConstraints,
        };
      }
    }

    const { suggestions, queryTimes } = await this.#store.queryWithMetrics(
      new lazy.SuggestionQuery({
        providers,
        keyword: searchString,
        providerConstraints: new lazy.SuggestionProviderConstraints(
          allProviderConstraints
        ),
      })
    );

    for (let { label, value } of queryTimes) {
      Glean.suggest.queryTime[label].accumulateSingleSample(value);
    }

    for (let suggestion of suggestions) {
      let type = getSuggestionType(suggestion);
      if (!type) {
        continue;
      }

      suggestion.source = "rust";
      suggestion.provider = type;
      if (suggestion.icon) {
        suggestion.icon_blob = new Blob([suggestion.icon], {
          type: suggestion.iconMimetype ?? "",
        });

        delete suggestion.icon;
        delete suggestion.iconMimetype;
      }
    }

    this.logger.debug("Got suggestions", suggestions);

    return suggestions;
  }

  cancelQuery() {
    this.#store?.interrupt(lazy.InterruptKind.READ);
  }

  /**
   * Returns suggestion-type-specific configuration data set by the Rust
   * backend.
   *
   * @param {string} type
   *   A Rust suggestion type name as defined in `suggest.udl`, e.g., "Amp",
   *   "Wikipedia", "Mdn", etc. See also `BaseFeature.rustSuggestionTypes`.
   * @returns {object} config
   *   The config data for the type.
   */
  getConfigForSuggestionType(type) {
    return this.#configsBySuggestionType.get(type);
  }

  /**
   * Ingests a feature's enabled suggestion types and updates staleness
   * bookkeeping. By default only stale suggestion types are ingested. A
   * suggestion type is stale if (a) it hasn't been ingested during this app
   * session or (b) the last time this method was called the suggestion type or
   * its feature was disabled.
   *
   * @param {BaseFeature} feature
   *   A feature that manages Rust suggestion types.
   * @param {object} options
   *   Options object.
   * @param {bool} options.evenIfFresh
   *   Set to true to force ingest for all the feature's suggestion types, even
   *   ones that aren't stale.
   */
  ingestEnabledSuggestions(feature, { evenIfFresh = false } = {}) {
    for (let type of feature.rustSuggestionTypes) {
      if (
        !this.isEnabled ||
        !feature.isEnabled ||
        !feature.isRustSuggestionTypeEnabled(type)
      ) {
        // Mark this type as stale so we'll ingest next time this method is
        // called.
        this.#providerConstraintsByIngestedSuggestionType.delete(type);
      } else {
        let providerConstraints = feature.getRustProviderConstraints(type);
        if (
          evenIfFresh ||
          !this.#providerConstraintsByIngestedSuggestionType.has(type) ||
          !lazy.ObjectUtils.deepEqual(
            providerConstraints,
            this.#providerConstraintsByIngestedSuggestionType.get(type)
          )
        ) {
          this.#providerConstraintsByIngestedSuggestionType.set(
            type,
            providerConstraints
          );
          this.#ingestSuggestionType({ type, providerConstraints });
        }
      }
    }
  }

  /**
   * Fetches geonames stored in the Suggest database. A geoname represents a
   * geographic place.
   *
   * See `SuggestStore::fetch_geonames()` in the Rust component for full
   * documentation.
   *
   * @param {string} searchString
   *   The string to match against geonames.
   * @param {bool} matchNamePrefix
   *   Whether prefix matching is performed on names excluding abbreviations and
   *   airport codes.
   * @param {GeonameType} geonameType
   *   Restricts returned geonames to a type.
   * @param {Array} filter
   *   Restricts returned geonames to certain cities or regions. Optional.
   * @returns {Array}
   *   Array of `GeonameMatch` objects. An empty array if there are no matches.
   */
  fetchGeonames(searchString, matchNamePrefix, geonameType, filter) {
    if (!this.#store) {
      return [];
    }
    return this.#store.fetchGeonames(
      searchString,
      matchNamePrefix,
      geonameType,
      filter
    );
  }

  /**
   * nsITimerCallback
   */
  notify() {
    this.logger.info("Ingest timer fired");
    this.#ingestAll();
  }

  get #storeDataPath() {
    return PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      SUGGEST_DATA_STORE_BASENAME
    );
  }

  /**
   * @returns {Array}
   *   Each item in this array identifies an enabled Rust suggestion type and
   *   related data. Items have the following properties:
   *
   *   {string} type
   *     A Rust suggestion type name as defined in Rust, e.g., "Amp",
   *     "Wikipedia", "Mdn", etc.
   *   {number} provider
   *     An integer that identifies the provider of the suggestion type to Rust.
   */
  get #enabledSuggestionTypes() {
    let items = [];
    for (let feature of lazy.QuickSuggest.rustFeatures) {
      if (feature.isEnabled) {
        for (let type of feature.rustSuggestionTypes) {
          if (feature.isRustSuggestionTypeEnabled(type)) {
            let provider = this.#providerFromSuggestionType(type);
            if (provider) {
              items.push({ type, provider });
            }
          }
        }
      }
    }
    return items;
  }

  #init() {
    // Initialize the store.
    this.logger.info("Initializing SuggestStore", {
      path: this.#storeDataPath,
    });
    let builder = lazy.SuggestStoreBuilder.init()
      .dataPath(this.#storeDataPath)
      .loadExtension(AppConstants.SQLITE_LIBRARY_FILENAME, "sqlite3_fts5_init")
      .remoteSettingsServer(this.#remoteSettingsServer)
      .remoteSettingsBucketName(this.#remoteSettingsBucketName);
    try {
      this.#store = builder.build();
    } catch (error) {
      this.logger.error("Error initializing SuggestStore", error);
      return;
    }

    // Log the last ingest time for debugging.
    let lastIngestSecs = Services.prefs.getIntPref(
      INGEST_TIMER_LAST_UPDATE_PREF,
      0
    );
    this.logger.debug("Last ingest time (seconds)", lastIngestSecs);

    // Interrupt any ongoing ingests (WRITE) and queries (READ) on shutdown.
    // Note that `interrupt()` runs on the main thread and is not async; see
    // toolkit/components/uniffi-bindgen-gecko-js/config.toml
    this.#shutdownBlocker = () =>
      this.#store?.interrupt(lazy.InterruptKind.READ_WRITE);
    lazy.AsyncShutdown.profileBeforeChange.addBlocker(
      "QuickSuggest: Interrupt the Rust component",
      this.#shutdownBlocker
    );

    // Register the ingest timer.
    lazy.timerManager.registerTimer(
      INGEST_TIMER_ID,
      this,
      lazy.UrlbarPrefs.get("quicksuggest.rustIngestIntervalSeconds"),
      true // skipFirst
    );

    // Do an initial ingest for all enabled suggestion types. When a type
    // becomes enabled after this point, its `BaseFeature` will update and call
    // `ingestEnabledSuggestions()`, which will be its initial ingest.
    this.#ingestAll();
  }

  #uninit() {
    this.#store = null;
    this.#providerConstraintsByIngestedSuggestionType.clear();
    this.#configsBySuggestionType.clear();
    lazy.timerManager.unregisterTimer(INGEST_TIMER_ID);

    lazy.AsyncShutdown.profileBeforeChange.removeBlocker(this.#shutdownBlocker);
    this.#shutdownBlocker = null;
  }

  /**
   * Ingests the given suggestion type.
   *
   * @param {object} options
   *   Options object.
   * @param {string} options.type
   *   A Rust suggestion type name as defined in `suggest.udl`, e.g., "Amp",
   *   "Wikipedia", "Mdn", etc.
   * @param {object|null} options.providerConstraints
   *   A plain JS object version of the type's provider constraints, if any.
   */
  #ingestSuggestionType({ type, providerConstraints }) {
    this.#ingestQueue.queueIdleCallback(async () => {
      if (!this.#store) {
        return;
      }

      let provider = this.#providerFromSuggestionType(type);
      if (!provider) {
        return;
      }

      let timerId;
      this.logger.debug("Starting ingest", { type });
      try {
        timerId = Glean.urlbar.quickSuggestIngestTime.start();
        const metrics = await this.#store.ingest(
          new lazy.SuggestIngestionConstraints({
            providers: [provider],
            providerConstraints: providerConstraints
              ? new lazy.SuggestionProviderConstraints(providerConstraints)
              : null,
          })
        );
        Glean.urlbar.quickSuggestIngestTime.stopAndAccumulate(timerId);
        for (let { label, value } of metrics.downloadTimes) {
          Glean.suggest.ingestDownloadTime[label].accumulateSingleSample(value);
        }
        for (let { label, value } of metrics.ingestionTimes) {
          Glean.suggest.ingestTime[label].accumulateSingleSample(value);
        }
      } catch (error) {
        // Ingest can throw a `SuggestApiError` subclass called `Other` with a
        // `reason` message, which is very helpful for diagnosing problems with
        // remote settings data in tests in particular.
        this.logger.error("Ingest error", {
          type,
          error,
          reason: error.reason,
        });
        Glean.urlbar.quickSuggestIngestTime.cancel(timerId);
      }
      this.logger.debug("Finished ingest", { type });

      if (!this.#store) {
        return;
      }

      // Fetch the provider config.
      this.logger.debug("Fetching provider config", { type });
      let config = await this.#store.fetchProviderConfig(provider);
      this.logger.debug("Got provider config", { type, config });
      this.#configsBySuggestionType.set(type, config);
      this.logger.debug("Finished fetching provider config", { type });
    });
  }

  #ingestAll() {
    // Ingest all enabled suggestion types.
    for (let feature of lazy.QuickSuggest.rustFeatures) {
      this.ingestEnabledSuggestions(feature, { evenIfFresh: true });
    }

    // Fetch the global config.
    this.#ingestQueue.queueIdleCallback(async () => {
      if (!this.#store) {
        return;
      }
      this.logger.debug("Fetching global config");
      this.#config = await this.#store.fetchGlobalConfig();
      this.logger.debug("Got global config", this.#config);
    });
  }

  /**
   * Given a Rust suggestion type, gets the integer value that identifies the
   * corresponding suggestion provider to Rust.
   *
   * @param {string} type
   *   A Rust suggestion type name as defined in `suggest.udl`, e.g., "Amp",
   *   "Wikipedia", "Mdn", etc.
   * @returns {number}
   *   An integer that identifies the provider of the suggestion type to Rust.
   */
  #providerFromSuggestionType(type) {
    let key = type.toUpperCase();
    if (!lazy.SuggestionProvider.hasOwnProperty(key)) {
      // Normally this shouldn't happen but it can during development when the
      // Rust component and desktop integration are out of sync.
      this.logger.error("SuggestionProvider[key] not defined!", { key });
      return null;
    }
    return lazy.SuggestionProvider[key];
  }

  #setRemoteSettingsConfig({ serverUrl, bucketName }) {
    this.#remoteSettingsServer = new lazy.RemoteSettingsServer.Custom(
      serverUrl
    );
    this.#remoteSettingsBucketName = bucketName;
  }

  get _test_store() {
    return this.#store;
  }

  get _test_enabledSuggestionTypes() {
    return this.#enabledSuggestionTypes;
  }

  async _test_setRemoteSettingsConfig({ serverUrl, bucketName }) {
    this.#setRemoteSettingsConfig({ serverUrl, bucketName });
    if (this.isEnabled) {
      // Recreate the store and re-ingest.
      Services.prefs.clearUserPref(INGEST_TIMER_LAST_UPDATE_PREF);
      this.#uninit();
      this.#init();
      await this.ingestPromise;
    }
  }

  async _test_ingest() {
    this.#ingestAll();
    await this.ingestPromise;
  }

  // The `SuggestStore` instance.
  #store;

  // Global Suggest config as returned from `SuggestStore.fetchGlobalConfig()`.
  #config = {};

  // Maps from suggestion type to provider config as returned from
  // `SuggestStore.fetchProviderConfig()`.
  #configsBySuggestionType = new Map();

  // Keeps track of suggestion types with fresh (non-stale) ingests. Maps
  // ingested suggestion types to `feature.getRustProviderConstraints(type)`.
  #providerConstraintsByIngestedSuggestionType = new Map();

  #ingestQueue;
  #shutdownBlocker;
  #remoteSettingsServer;
  #remoteSettingsBucketName;
}

/**
 * Returns the type of a suggestion.
 *
 * @param {Suggestion} suggestion
 *   A suggestion object, an instance of one of the `Suggestion` subclasses.
 * @returns {string}
 *   The suggestion's type, e.g., "Amp", "Wikipedia", etc.
 */
function getSuggestionType(suggestion) {
  // Suggestion objects served by the Rust component don't have any inherent
  // type information other than the classes they are instances of. There's no
  // `type` property, for example. There's a base `Suggestion` class and many
  // `Suggestion` subclasses, one per type of suggestion. Each suggestion object
  // is an instance of one of these subclasses. We derive a suggestion's type
  // from the subclass it's an instance of.
  //
  // Unfortunately the subclasses are all anonymous, which means
  // `suggestion.constructor.name` is always an empty string. (This is due to
  // how UniFFI generates JS bindings.) Instead, the subclasses are defined as
  // properties on the base `Suggestion` class. For example,
  // `Suggestion.Wikipedia` is the (anonymous) Wikipedia suggestion class. To
  // find a suggestion's subclass, we loop through the keys on `Suggestion`
  // until we find the value the suggestion is an instance of. To avoid doing
  // this every time, we cache the mapping from suggestion constructor to key
  // the first time we encounter a new suggestion subclass.
  let type = gSuggestionTypesByCtor.get(suggestion.constructor);
  if (!type) {
    type = Object.keys(lazy.Suggestion).find(
      key => suggestion instanceof lazy.Suggestion[key]
    );
    if (type) {
      gSuggestionTypesByCtor.set(suggestion.constructor, type);
    } else {
      console.error(
        "Unexpected error: Suggestion class not found on `Suggestion`. " +
          "Did the Rust component or its JS bindings change? ",
        { suggestion }
      );
    }
  }
  return type;
}
