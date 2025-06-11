/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

// See the `QuickSuggest.SETTINGS_UI` jsdoc below.
const SETTINGS_UI = Object.freeze({
  FULL: 0,
  NONE: 1,
  // Only settings relevant to offline will be shown. Settings that pertain to
  // online will be hidden.
  OFFLINE_ONLY: 2,
});

/**
 * @typedef {[string[], boolean|number]} RegionLocaleDefault
 *   The first element is an array of locale prefixes (e.g. "en"), the second is
 *   the value of the preference.
 */

/**
 * @typedef {object} SuggestPrefsRecord
 * @property {Record<string, RegionLocaleDefault>} [defaultValues]
 *   This controls the home regions and locales where Suggest and each of its
 *   subfeatures will be enabled. If the pref should be initialized on the
 *   default branch depending on the user's home region and locale, then this
 *   should be set to an object where each entry maps a region name to a tuple
 *   `[localePrefixes, prefValue]`. `localePrefixes` is an array of strings and
 *   `prefValue` is the value that should be set when the region and locale
 *   prefixes match the user's region and locale. If the user's region and
 *   locale do not match any of the entries in `defaultValues`, then the pref
 *   will retain its default value as defined in `firefox.js`.
 * @property {string} [nimbusVariableIfExposedInUi]
 *   If the pref is exposed in the settings UI and it's a fallback for a Nimbus
 *   variable, then this should be set to the variable's name. See point 3 in
 *   the comment in `#initDefaultPrefs()` for more.
 */

/**
 * This defines the home regions and locales where Suggest will be enabled.
 * Suggest will remain disabled for regions and locales not defined here. More
 * generally it defines important Suggest prefs that require special handling.
 * Each entry in this object defines a pref name and information about that
 * pref. Pref names are relative to `browser.urlbar.` The value in each entry is
 * an object with the following properties:
 *
 * @type {{[key: string]: SuggestPrefsRecord}}
 * {object} defaultValues
 */
const SUGGEST_PREFS = Object.freeze({
  // Prefs related to Suggest overall
  "quicksuggest.dataCollection.enabled": {
    nimbusVariableIfExposedInUi: "quickSuggestDataCollectionEnabled",
  },
  "quicksuggest.enabled": {
    defaultValues: {
      GB: [["en"], true],
      US: [["en"], true],
    },
  },
  "quicksuggest.settingsUi": {
    defaultValues: {
      GB: [["en"], SETTINGS_UI.OFFLINE_ONLY],
      US: [["en"], SETTINGS_UI.FULL],
    },
  },
  "suggest.quicksuggest.nonsponsored": {
    nimbusVariableIfExposedInUi: "quickSuggestNonSponsoredEnabled",
    defaultValues: {
      GB: [["en"], true],
      US: [["en"], true],
    },
  },
  "suggest.quicksuggest.sponsored": {
    nimbusVariableIfExposedInUi: "quickSuggestSponsoredEnabled",
    defaultValues: {
      GB: [["en"], true],
      US: [["en"], true],
    },
  },

  // Prefs related to individual features
  "addons.featureGate": {
    defaultValues: {
      US: [["en"], true],
    },
  },
  "mdn.featureGate": {
    defaultValues: {
      US: [["en"], true],
    },
  },
  "weather.featureGate": {
    defaultValues: {
      GB: [["en"], true],
      US: [["en"], true],
    },
  },
  "yelp.featureGate": {
    defaultValues: {
      US: [["en"], true],
    },
  },
});

// Suggest features classes. On init, `QuickSuggest` creates an instance of each
// class and keeps it in the `#featuresByName` map. See `SuggestFeature`.
const FEATURES = {
  AddonSuggestions:
    "resource:///modules/urlbar/private/AddonSuggestions.sys.mjs",
  AmpSuggestions: "resource:///modules/urlbar/private/AmpSuggestions.sys.mjs",
  FakespotSuggestions:
    "resource:///modules/urlbar/private/FakespotSuggestions.sys.mjs",
  DynamicSuggestions:
    "resource:///modules/urlbar/private/DynamicSuggestions.sys.mjs",
  ImpressionCaps: "resource:///modules/urlbar/private/ImpressionCaps.sys.mjs",
  MDNSuggestions: "resource:///modules/urlbar/private/MDNSuggestions.sys.mjs",
  OfflineWikipediaSuggestions:
    "resource:///modules/urlbar/private/OfflineWikipediaSuggestions.sys.mjs",
  SuggestBackendMerino:
    "resource:///modules/urlbar/private/SuggestBackendMerino.sys.mjs",
  SuggestBackendMl:
    "resource:///modules/urlbar/private/SuggestBackendMl.sys.mjs",
  SuggestBackendRust:
    "resource:///modules/urlbar/private/SuggestBackendRust.sys.mjs",
  WeatherSuggestions:
    "resource:///modules/urlbar/private/WeatherSuggestions.sys.mjs",
  YelpSuggestions: "resource:///modules/urlbar/private/YelpSuggestions.sys.mjs",
};

/**
 * @import {SuggestBackendRust} from "resource:///modules/urlbar/private/SuggestBackendRust.sys.mjs"
 * @import {SuggestFeature} from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs"
 * @import {SuggestProvider} from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs"
 * @import {ImpressionCaps} from "resource:///modules/urlbar/private/ImpressionCaps.sys.mjs"
 */

/**
 * This class manages Firefox Suggest and has related helpers.
 */
class _QuickSuggest {
  /**
   * Test-only variable to skip telemetry environment initialisation.
   */
  _testSkipTelemetryEnvironmentInit = false;

  /**
   * @returns {string}
   *   The help URL for Suggest.
   */
  get HELP_URL() {
    return (
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "firefox-suggest"
    );
  }

  /**
   * @returns {object}
   *   Possible values of the `quickSuggestSettingsUi` Nimbus variable and its
   *   fallback pref `browser.urlbar.quicksuggest.settingsUi`. When Suggest is
   *   enabled, these values determine the Suggest settings that will be visible
   *   in `about:preferences`. When Suggest is disabled, the variable/pref are
   *   ignored and Suggest settings are hidden.
   */
  get SETTINGS_UI() {
    return SETTINGS_UI;
  }

  /**
   * @returns {Promise}
   *   Resolved when Suggest initialization finishes.
   */
  get initPromise() {
    return this.#initResolvers.promise;
  }

  /**
   * @returns {Array}
   *   Enabled Suggest backends.
   */
  get enabledBackends() {
    // This getter may be accessed before `init()` is called, so the backends
    // may not be registered yet. Don't assume they're non-null.
    return [
      this.rustBackend,
      this.#featuresByName.get("SuggestBackendMerino"),
      this.#featuresByName.get("SuggestBackendMl"),
    ].filter(b => b?.isEnabled);
  }

  /**
   * @returns {SuggestBackendRust}
   *   The Rust backend, which manages the Rust component.
   */
  get rustBackend() {
    return this.#featuresByName.get("SuggestBackendRust");
  }

  /**
   * @returns {object}
   *   Global Suggest configuration stored in remote settings and ingested by
   *   the Rust component. See remote settings or the Rust component for the
   *   latest schema.
   */
  get config() {
    return this.rustBackend?.config || {};
  }

  /**
   * @returns {ImpressionCaps}
   *   The impression caps feature.
   */
  get impressionCaps() {
    return this.#featuresByName.get("ImpressionCaps");
  }

  /**
   * @returns {Set}
   *   The set of features that manage Rust suggestion types, as determined by
   *   each feature's `rustSuggestionType`.
   */
  get rustFeatures() {
    return new Set(this.#featuresByRustSuggestionType.values());
  }

  /**
   * @returns {Set}
   *   The set of features that manage ML suggestion types, as determined by
   *   each feature's `mlIntent`.
   */
  get mlFeatures() {
    return new Set(this.#featuresByMlIntent.values());
  }

  get logger() {
    if (!this._logger) {
      this._logger = lazy.UrlbarUtils.getLogger({ prefix: "QuickSuggest" });
    }
    return this._logger;
  }

  /**
   * Initializes Suggest. It's safe to call more than once.
   *
   * @param {object} testOverrides
   *   This is intended for tests only. See `#initDefaultPrefs()`.
   */
  async init(testOverrides = null) {
    if (this.#initStarted) {
      await this.initPromise;
      return;
    }
    this.#initStarted = true;

    // Wait for dependencies to finish before initializing prefs.
    //
    // (1) Whether Suggest should be enabled depends on the user's region.
    await lazy.Region.init();

    // (2) The default-branch values of Suggest prefs that are both exposed in
    // the UI and configurable by Nimbus depend on Nimbus.
    await lazy.NimbusFeatures.urlbar.ready();

    // (3) `TelemetryEnvironment` records the values of some Suggest prefs.
    if (!this._testSkipTelemetryEnvironmentInit) {
      await lazy.TelemetryEnvironment.onInitialized();
    }

    this.#initDefaultPrefs(testOverrides);

    // Create an instance of each feature and keep it in `#featuresByName`.
    for (let [name, uri] of Object.entries(FEATURES)) {
      let { [name]: ctor } = ChromeUtils.importESModule(uri);
      let feature = new ctor();
      this.#featuresByName.set(name, feature);
      if (feature.merinoProvider) {
        this.#featuresByMerinoProvider.set(feature.merinoProvider, feature);
      }
      if (feature.rustSuggestionType) {
        this.#featuresByRustSuggestionType.set(
          feature.rustSuggestionType,
          feature
        );
      }
      if (feature.mlIntent) {
        this.#featuresByMlIntent.set(feature.mlIntent, feature);
      }

      // Update the map from enabling preferences to features.
      let prefs = feature.enablingPreferences;
      if (prefs) {
        for (let p of prefs) {
          let features = this.#featuresByEnablingPrefs.get(p);
          if (!features) {
            features = new Set();
            this.#featuresByEnablingPrefs.set(p, features);
          }
          features.add(feature);
        }
      }
    }

    this.#updateAll();
    lazy.UrlbarPrefs.addObserver(this);

    this.#initResolvers.resolve();
  }

  /**
   * Returns a Suggest feature by name.
   *
   * @param {string} name
   *   The name of the feature's JS class.
   * @returns {SuggestFeature}
   *   The feature object, an instance of a subclass of `SuggestFeature`.
   */
  getFeature(name) {
    return this.#featuresByName.get(name);
  }

  /**
   * Returns a Suggest feature by the name of the Merino provider that serves
   * its suggestions (as defined by `feature.merinoProvider`). Not all features
   * correspond to a Merino provider.
   *
   * @param {string} provider
   *   The name of a Merino provider.
   * @returns {SuggestProvider}
   *   The feature object, an instance of a subclass of `SuggestProvider`, or
   *   null if no feature corresponds to the Merino provider.
   */
  getFeatureByMerinoProvider(provider) {
    return this.#featuresByMerinoProvider.get(provider);
  }

  /**
   * Returns a Suggest feature by the type of Rust suggestion it manages (as
   * defined by `feature.rustSuggestionType`). Not all features correspond to a
   * Rust suggestion type.
   *
   * @param {string} type
   *   The name of a Rust suggestion type.
   * @returns {SuggestProvider}
   *   The feature object, an instance of a subclass of `SuggestProvider`, or
   *   null if no feature corresponds to the type.
   */
  getFeatureByRustSuggestionType(type) {
    return this.#featuresByRustSuggestionType.get(type);
  }

  /**
   * Returns a Suggest feature by the ML intent name (as defined by
   * `feature.mlIntent` and `MLSuggest`). Not all features support ML.
   *
   * @param {string} intent
   *   The name of an ML intent.
   * @returns {SuggestProvider}
   *   The feature object, an instance of a subclass of `SuggestProvider`, or
   *   null if no feature corresponds to the intent.
   */
  getFeatureByMlIntent(intent) {
    return this.#featuresByMlIntent.get(intent);
  }

  /**
   * Gets the Suggest feature that manages suggestions for urlbar result.
   *
   * @param {UrlbarResult} result
   *   The urlbar result.
   * @returns {SuggestProvider}
   *   The feature instance or null if none was found.
   */
  getFeatureByResult(result) {
    return this.getFeatureBySource(result.payload);
  }

  /**
   * Gets the Suggest feature that manages suggestions for a source and provider
   * name. The source and provider name can be supplied from either a suggestion
   * object or the payload of a `UrlbarResult` object.
   *
   * @param {object} options
   *   Options object.
   * @param {string} options.source
   *   The suggestion source, one of: "merino", "ml", "rust"
   * @param {string} options.provider
   *   This value depends on `source`. The possible values per source are:
   *
   *   merino:
   *     The name of the Merino provider that serves the suggestion type
   *   ml:
   *     The name of the intent as determined by `MLSuggest`
   *   rust:
   *     The name of the suggestion type as defined in Rust
   * @returns {SuggestProvider}
   *   The feature instance or null if none was found.
   */
  getFeatureBySource({ source, provider }) {
    switch (source) {
      case "merino":
        return this.getFeatureByMerinoProvider(provider);
      case "rust":
        return this.getFeatureByRustSuggestionType(provider);
      case "ml":
        return this.getFeatureByMlIntent(provider);
    }
    return null;
  }

  /**
   * Registers a dismissal with the Rust backend. A
   * `quicksuggest-dismissals-changed` notification topic is sent when done.
   *
   * @param {UrlbarResult} result
   *   The result to dismiss.
   */
  async dismissResult(result) {
    if (result.payload.source == "rust") {
      await this.rustBackend?.dismissRustSuggestion(
        result.payload.suggestionObject
      );
    } else {
      let key = getDismissalKey(result);
      if (key) {
        await this.rustBackend?.dismissByKey(key);
      }
    }

    Services.obs.notifyObservers(null, "quicksuggest-dismissals-changed");
  }

  /**
   * Returns whether a dismissal is recorded for a result.
   *
   * @param {UrlbarResult} result
   *   The result to check.
   * @returns {Promise<boolean>}
   *   Whether the result has been dismissed.
   */
  async isResultDismissed(result) {
    let promises = [
      // Check whether the result was dismissed using the old API, where
      // dismissals were recorded as URL digests.
      getDigest(result.payload.originalUrl || result.payload.url).then(digest =>
        this.rustBackend?.isDismissedByKey(digest)
      ),
    ];

    if (result.payload.source == "rust") {
      promises.push(
        this.rustBackend?.isRustSuggestionDismissed(
          result.payload.suggestionObject
        )
      );
    } else {
      let key = getDismissalKey(result);
      if (key) {
        promises.push(this.rustBackend?.isDismissedByKey(key));
      }
    }

    let values = await Promise.all(promises);
    return values.some(v => !!v);
  }

  /**
   * Clears all dismissed suggestions, including individually dismissed
   * suggestions and dismissed suggestion types. The following notification
   * topics are sent when done, in this order:
   *
   * ```
   * quicksuggest-dismissals-changed
   * quicksuggest-dismissals-cleared
   * ```
   */
  async clearDismissedSuggestions() {
    // Clear the user value of each feature's primary user-controlled pref if
    // its value is `false`.
    for (let [name, feature] of this.#featuresByName) {
      let pref = feature.primaryUserControlledPreference;
      // This should never throw, but try-catch to avoid breaking the entire
      // loop if `UrlbarPrefs` doesn't recognize a pref in one iteration.
      try {
        if (pref && !lazy.UrlbarPrefs.get(pref)) {
          lazy.UrlbarPrefs.clear(pref);
        }
      } catch (error) {
        this.logger.error("Error clearing primaryEnablingPreference", {
          "feature.name": name,
          pref,
          error,
        });
      }
    }

    // Clear individually dismissed suggestions, which are stored in the Rust
    // component regardless of their source.
    await this.rustBackend?.clearDismissedSuggestions();

    Services.obs.notifyObservers(null, "quicksuggest-dismissals-changed");
    Services.obs.notifyObservers(null, "quicksuggest-dismissals-cleared");
  }

  /**
   * Whether there are any dismissed suggestions that can be cleared, including
   * individually dismissed suggestions and dismissed suggestion types.
   *
   * @returns {Promise<boolean>}
   *   Whether dismissals can be cleared.
   */
  async canClearDismissedSuggestions() {
    // Return true if any feature's primary user-controlled pref is `false` on
    // the user branch.
    for (let [name, feature] of this.#featuresByName) {
      let pref = feature.primaryUserControlledPreference;
      // This should never throw, but try-catch to avoid breaking the entire
      // loop if `UrlbarPrefs` doesn't recognize a pref in one iteration.
      try {
        if (
          pref &&
          !lazy.UrlbarPrefs.get(pref) &&
          lazy.UrlbarPrefs.hasUserValue(pref)
        ) {
          return true;
        }
      } catch (error) {
        this.logger.error("Error accessing primaryUserControlledPreference", {
          "feature.name": name,
          pref,
          error,
        });
      }
    }

    // Return true if there are any individually dismissed suggestions.
    if (await this.rustBackend?.anyDismissedSuggestions()) {
      return true;
    }

    return false;
  }

  /**
   * Gets the intended default Suggest prefs for a home region and locale.
   *
   * @param {string} region
   *   A home region, typically from `Region.home`.
   * @param {string} locale
   *   A locale.
   * @returns {object}
   *   An object that maps pref names to their intended default values. Pref
   *   names are relative to `browser.urlbar.`.
   */
  intendedDefaultPrefs(region, locale) {
    let regionLocalePrefs = Object.fromEntries(
      Object.entries(SUGGEST_PREFS)
        .map(([prefName, { defaultValues }]) => {
          if (defaultValues?.hasOwnProperty(region)) {
            let [localePrefixes, prefValue] = defaultValues[region];
            if (localePrefixes.some(p => locale.startsWith(p))) {
              return [prefName, prefValue];
            }
          }
          return null;
        })
        .filter(entry => !!entry)
    );
    return {
      ...this.#unmodifiedDefaultPrefs,
      ...regionLocalePrefs,
    };
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    // If any feature's enabling preferences changed, update it now.
    let features = this.#featuresByEnablingPrefs.get(pref);
    if (!features) {
      return;
    }

    let isPrimaryUserControlledPref = false;

    for (let f of features) {
      f.update();
      if (pref == f.primaryUserControlledPreference) {
        isPrimaryUserControlledPref = true;
      }
    }

    if (isPrimaryUserControlledPref) {
      Services.obs.notifyObservers(null, "quicksuggest-dismissals-changed");
    }
  }

  /**
   * Called when a urlbar Nimbus variable changes.
   *
   * @param {string} variable
   *   The name of the variable.
   */
  onNimbusChanged(variable) {
    // If a change occurred to a variable that corresponds to a pref exposed in
    // the UI, sync the variable to the pref on the default branch.
    this.#syncNimbusVariablesToUiPrefs(variable);

    // Update features.
    this.#updateAll();
  }

  /**
   * Returns whether a given URL and result URL map back to the same original
   * suggestion URL.
   *
   * Some features may create result URLs that are potentially unique per query.
   * Typically this is done by modifying an original suggestion URL at query
   * time, for example by adding timestamps or query-specific search params. In
   * that case, a single original suggestion URL will map to many result URLs.
   * This function returns whether the given URL and result URL are equal
   * excluding any such modifications.
   *
   * @param {string} url
   *   The URL to check, typically from the user's history.
   * @param {UrlbarResult} result
   *   The Suggest result.
   * @returns {boolean}
   *   Whether `url` is equivalent to the result's URL.
   */
  isUrlEquivalentToResultUrl(url, result) {
    let feature = this.getFeatureByResult(result);
    return feature
      ? feature.isUrlEquivalentToResultUrl(url, result)
      : url == result.payload.url;
  }

  /**
   * @returns {object}
   *   An object that maps from Nimbus variable names to their corresponding
   *   prefs, for prefs in `SUGGEST_PREFS` with `nimbusVariableIfExposedInUi`
   *   set.
   */
  get #uiPrefsByNimbusVariable() {
    return Object.fromEntries(
      Object.entries(SUGGEST_PREFS)
        .map(([prefName, { nimbusVariableIfExposedInUi }]) =>
          nimbusVariableIfExposedInUi
            ? [nimbusVariableIfExposedInUi, prefName]
            : null
        )
        .filter(entry => !!entry)
    );
  }

  /**
   * Sets appropriate default-branch values of Suggest prefs depending on
   * whether Suggest should be enabled by default.
   *
   * @param {object} testOverrides
   *   This is intended for tests only. Pass to force the following:
   *   `{ region, locale, migrationVersion, defaultPrefs }`
   */
  #initDefaultPrefs(testOverrides = null) {
    // Updating prefs is tricky and it's important to preserve the user's
    // choices, so we describe the process in detail below. tl;dr:
    //
    // * Prefs exposed in the settings UI should be sticky.
    // * Prefs that are both exposed in the settings UI and configurable via
    //   Nimbus should be added to `SUGGEST_PREFS` with
    //   `nimbusVariableIfExposedInUi` set appropriately.
    // * Prefs with `nimbusVariableIfExposedInUi` set should not be specified as
    //   `fallbackPref` for their Nimbus variables. Access these prefs directly
    //   instead of through their variables.
    //
    // The pref-update process is described next.
    //
    // 1. Determine the appropriate values for Suggest prefs according to the
    //    user's home region and locale.
    //
    // 2. Set the prefs on the default branch. We use the default branch and not
    //    the user branch because we want to distinguish default prefs from the
    //    user's choices.
    //
    //    In particular it's important to consider prefs that are exposed in the
    //    UI, like whether sponsored suggestions are enabled. Once the user
    //    makes a choice to change a default, we want to preserve that choice
    //    indefinitely regardless of whether Suggest is currently enabled or
    //    will be enabled in the future. User choices are of course recorded on
    //    the user branch, so if we set defaults on the user branch too, we
    //    wouldn't be able to distinguish user choices from default values. This
    //    is also why prefs that are exposed in the UI should be sticky. Unlike
    //    non-sticky prefs, sticky prefs retain their user-branch values even
    //    when those values are the same as the ones on the default branch.
    //
    //    It's important to note that the defaults we set here do not persist
    //    across app restarts. (This is a feature of the pref service; prefs set
    //    programmatically on the default branch are not stored anywhere
    //    permanent like firefox.js or user.js.) That's why BrowserGlue calls
    //    `init()` on every startup.
    //
    // 3. Some prefs are both exposed in the UI and configurable via Nimbus,
    //    like whether data collection is enabled. We absolutely want to
    //    preserve the user's past choices for these prefs. But if the user
    //    hasn't yet made a choice for a particular pref, then it should be
    //    configurable.
    //
    //    For any such prefs that have values defined in Nimbus, we set their
    //    default-branch values to their Nimbus values. (These defaults
    //    therefore override any set in the previous step.) If a pref has a user
    //    value, accessing the pref will return the user value; if it does not
    //    have a user value, accessing it will return the value that was
    //    specified in Nimbus.
    //
    //    This isn't strictly necessary. Since prefs exposed in the UI are
    //    sticky, they will always preserve their user-branch values regardless
    //    of their default-branch values, and as long as a pref is listed as a
    //    `fallbackPref` for its corresponding Nimbus variable, Nimbus will use
    //    the user-branch value. So we could instead specify fallback prefs in
    //    Nimbus and always access values through Nimbus instead of through
    //    prefs. But that would make preferences UI code a little harder to
    //    write since the checked state of a checkbox would depend on something
    //    other than its pref. Since we're already setting default-branch values
    //    here as part of the previous step, it's not much more work to set
    //    defaults for these prefs too, and it makes the UI code a little nicer.
    //
    // 4. Migrate prefs as necessary. This refers to any pref changes that are
    //    neccesary across app versions: introducing and initializing new prefs,
    //    removing prefs, or changing the meaning of existing prefs.

    // We use `Preferences` because it lets us access prefs without worrying
    // about their types and can do so on the default branch. Most of our prefs
    // are bools but not all.
    let defaults = new lazy.Preferences({
      branch: "browser.urlbar.",
      defaultBranch: true,
    });

    // Before setting defaults, save their original unmodifed values as defined
    // in `firefox.js` so we can restore them if Suggest becomes disabled.
    if (!this.#unmodifiedDefaultPrefs) {
      this.#unmodifiedDefaultPrefs = Object.fromEntries(
        Object.keys(SUGGEST_PREFS).map(name => [name, defaults.get(name)])
      );
    }

    // 1. Determine the appropriate values for Suggest prefs according to the
    //    user's home region and locale.
    if (testOverrides?.defaultPrefs) {
      this.#intendedDefaultPrefs = testOverrides.defaultPrefs;
    } else {
      let region = testOverrides?.region ?? lazy.Region.home;
      let locale = testOverrides?.locale ?? Services.locale.appLocaleAsBCP47;
      this.#intendedDefaultPrefs = this.intendedDefaultPrefs(region, locale);
    }

    // 2. Set the prefs on the default branch.
    for (let [name, value] of Object.entries(this.#intendedDefaultPrefs)) {
      defaults.set(name, value);
    }

    // 3. Set default-branch values for prefs that are both exposed in the
    // settings UI and configurable via Nimbus.
    this.#syncNimbusVariablesToUiPrefs();

    // 4. Migrate prefs across app versions.
    let shouldEnableSuggest =
      !!this.#intendedDefaultPrefs["quicksuggest.enabled"];
    this._ensureFirefoxSuggestPrefsMigrated(shouldEnableSuggest, testOverrides);
  }

  /**
   * Sets default-branch values for prefs in `#uiPrefsByNimbusVariable`, i.e.,
   * prefs that are both exposed in the settings UI and configurable via Nimbus.
   *
   * @param {string} variable
   *   If defined, only the pref corresponding to this variable will be set. If
   *   there is no UI pref for this variable, this function is a no-op.
   */
  #syncNimbusVariablesToUiPrefs(variable = null) {
    let prefsByVariable = this.#uiPrefsByNimbusVariable;

    if (variable) {
      if (!prefsByVariable.hasOwnProperty(variable)) {
        // `variable` does not correspond to a pref exposed in the UI.
        return;
      }
      // Restrict `prefsByVariable` only to `variable`.
      prefsByVariable = { [variable]: prefsByVariable[variable] };
    }

    let defaults = new lazy.Preferences({
      branch: "browser.urlbar.",
      defaultBranch: true,
    });

    for (let [v, pref] of Object.entries(prefsByVariable)) {
      let value = lazy.NimbusFeatures.urlbar.getVariable(v);
      if (value === undefined) {
        value = this.#intendedDefaultPrefs[pref];
      }
      defaults.set(pref, value);
    }
  }

  /**
   * Updates all features.
   */
  #updateAll() {
    // IMPORTANT: This method is a `NimbusFeatures.urlbar.onUpdate()` callback,
    // which means it's called on every change to any pref that is a fallback
    // for a urlbar Nimbus variable.

    // Update features.
    for (let feature of this.#featuresByName.values()) {
      feature.update();
    }
  }

  /**
   * The current version of the Firefox Suggest prefs.
   *
   * @returns {number}
   */
  get MIGRATION_VERSION() {
    return 2;
  }

  /**
   * Migrates Firefox Suggest prefs to the current version if they haven't been
   * migrated already.
   *
   * @param {boolean} shouldEnableSuggest
   *   Whether Suggest should be enabled right now.
   * @param {object} testOverrides
   *   This is intended for tests only. Pass to force a migration version:
   *   `{ migrationVersion }`
   */
  _ensureFirefoxSuggestPrefsMigrated(shouldEnableSuggest, testOverrides) {
    let currentVersion =
      testOverrides?.migrationVersion !== undefined
        ? testOverrides.migrationVersion
        : this.MIGRATION_VERSION;
    let lastSeenVersion = Math.max(
      0,
      lazy.UrlbarPrefs.get("quicksuggest.migrationVersion")
    );
    if (currentVersion <= lastSeenVersion) {
      // Migration up to date.
      return;
    }

    // Migrate from the last-seen version up to the current version.
    let version = lastSeenVersion;
    for (; version < currentVersion; version++) {
      let nextVersion = version + 1;
      let methodName = "_migrateFirefoxSuggestPrefsTo_" + nextVersion;
      try {
        this[methodName](shouldEnableSuggest);
      } catch (error) {
        console.error(
          `Error migrating Firefox Suggest prefs to version ${nextVersion}:`,
          error
        );
        break;
      }
    }

    // Record the new last-seen migration version.
    lazy.UrlbarPrefs.set("quicksuggest.migrationVersion", version);
  }

  _migrateFirefoxSuggestPrefsTo_1(shouldEnableSuggest) {
    // Copy `suggest.quicksuggest` to `suggest.quicksuggest.nonsponsored` and
    // clear the first.
    let suggestQuicksuggest = "browser.urlbar.suggest.quicksuggest";
    if (Services.prefs.prefHasUserValue(suggestQuicksuggest)) {
      lazy.UrlbarPrefs.set(
        "suggest.quicksuggest.nonsponsored",
        Services.prefs.getBoolPref(suggestQuicksuggest)
      );
      Services.prefs.clearUserPref(suggestQuicksuggest);
    }

    // In the unversioned prefs, sponsored suggestions were shown only if the
    // main suggestions pref `suggest.quicksuggest` was true, but now there are
    // two independent prefs, so disable sponsored if the main pref was false.
    if (
      shouldEnableSuggest &&
      !lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")
    ) {
      // Set the pref on the user branch. Suggestions are enabled by default
      // for offline; we want to preserve the user's choice of opting out,
      // and we want to preserve the default-branch true value.
      lazy.UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
    }
  }

  _migrateFirefoxSuggestPrefsTo_2() {
    // In previous versions of the prefs for online, suggestions were disabled
    // by default; in version 2, they're enabled by default. For users who were
    // already in online and did not enable suggestions (because they did not
    // opt in, they did opt in but later disabled suggestions, or they were not
    // shown the modal) we don't want to suddenly enable them, so if the prefs
    // do not have user-branch values, set them to false.
    let scenario = Services.prefs.getCharPref(
      "browser.urlbar.quicksuggest.scenario",
      ""
    );
    if (scenario == "online") {
      if (
        !Services.prefs.prefHasUserValue(
          "browser.urlbar.suggest.quicksuggest.nonsponsored"
        )
      ) {
        lazy.UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
      }
      if (
        !Services.prefs.prefHasUserValue(
          "browser.urlbar.suggest.quicksuggest.sponsored"
        )
      ) {
        lazy.UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
      }
    }
  }

  async _test_reinit(testOverrides = null) {
    if (this.#initStarted) {
      await this.initPromise;
      this.#initStarted = false;
      this.#initResolvers = Promise.withResolvers();
    }

    if (this.rustBackend) {
      // Make sure to await any queued ingests before re-initializing.  Otherwise there could be a race
      // between when that ingestion finishes and when the test finishes and calls
      // `SharedRemoteSettingsService.updateServer()` to reset the remote settings server.
      await this.rustBackend.ingestPromise;
    }

    await this.init(testOverrides);
  }

  #initStarted = false;
  #initResolvers = Promise.withResolvers();

  // Maps from Suggest feature class names to feature instances.
  #featuresByName = new Map();

  // Maps from Merino provider names to Suggest feature instances.
  #featuresByMerinoProvider = new Map();

  // Maps from Rust suggestion types to Suggest feature instances.
  #featuresByRustSuggestionType = new Map();

  // Maps from ML intent strings to Suggest feature instances.
  #featuresByMlIntent = new Map();

  // Maps from preference names to the `Set` of feature instances they enable.
  #featuresByEnablingPrefs = new Map();

  // A plain JS object that maps pref names relative to `browser.urlbar.` to
  // their intended defaults depending on whether Suggest should be enabled.
  #intendedDefaultPrefs;

  // A plain JS object that maps pref names relative to `browser.urlbar.` to
  // their original unmodified values as defined in `firefox.js`.
  #unmodifiedDefaultPrefs;
}

function getDismissalKey(result) {
  return (
    result.payload.dismissalKey ||
    result.payload.originalUrl ||
    result.payload.url
  );
}

async function getDigest(string) {
  let stringArray = new TextEncoder().encode(string);
  let hashBuffer = await crypto.subtle.digest("SHA-1", stringArray);
  let hashArray = new Uint8Array(hashBuffer);
  return Array.from(hashArray, b => b.toString(16).padStart(2, "0")).join("");
}

export const QuickSuggest = new _QuickSuggest();
