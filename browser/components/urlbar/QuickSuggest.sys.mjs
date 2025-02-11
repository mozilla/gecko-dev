/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

// Suggest features classes. On init, `QuickSuggest` creates an instance of each
// class and keeps it in the `#featuresByName` map. See `SuggestFeature`.
const FEATURES = {
  AddonSuggestions:
    "resource:///modules/urlbar/private/AddonSuggestions.sys.mjs",
  AmpSuggestions: "resource:///modules/urlbar/private/AmpSuggestions.sys.mjs",
  BlockedSuggestions:
    "resource:///modules/urlbar/private/BlockedSuggestions.sys.mjs",
  ExposureSuggestions:
    "resource:///modules/urlbar/private/ExposureSuggestions.sys.mjs",
  FakespotSuggestions:
    "resource:///modules/urlbar/private/FakespotSuggestions.sys.mjs",
  ImpressionCaps: "resource:///modules/urlbar/private/ImpressionCaps.sys.mjs",
  MDNSuggestions: "resource:///modules/urlbar/private/MDNSuggestions.sys.mjs",
  OfflineWikipediaSuggestions:
    "resource:///modules/urlbar/private/OfflineWikipediaSuggestions.sys.mjs",
  PocketSuggestions:
    "resource:///modules/urlbar/private/PocketSuggestions.sys.mjs",
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

// Values returned by the onboarding dialog depending on the user's response.
// These values are used in telemetry events, so be careful about changing them.
const ONBOARDING_CHOICE = {
  ACCEPT_2: "accept_2",
  CLOSE_1: "close_1",
  DISMISS_1: "dismiss_1",
  DISMISS_2: "dismiss_2",
  LEARN_MORE_1: "learn_more_1",
  LEARN_MORE_2: "learn_more_2",
  NOT_NOW_2: "not_now_2",
  REJECT_2: "reject_2",
};

const ONBOARDING_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.html";

/**
 * This class manages Firefox Suggest and has related helpers.
 */
class _QuickSuggest {
  /**
   * Default prefs relative to `browser.urlbar` per Firefox Suggest scenario.
   *
   * @returns {object}
   */
  get DEFAULT_PREFS() {
    // Important notes when modifying this:
    //
    // If you add a pref to one scenario, you typically need to add it to all
    // scenarios even if the pref is in firefox.js. That's because we need to
    // allow for switching from one scenario to another at any time after
    // startup. If we set a pref for one scenario on the default branch, we
    // switch to a new scenario, and we don't set the pref for the new scenario,
    // it will keep its default-branch value from the old scenario. The only
    // possible exception is for prefs that make others unnecessary, like how
    // when `quicksuggest.enabled` is false, none of the other prefs matter.
    //
    // Prefs not listed here for any scenario keep their values set in
    // firefox.js.
    return {
      history: {
        "quicksuggest.enabled": false,
        "quicksuggest.dataCollection.enabled": false,
        "suggest.quicksuggest.nonsponsored": false,
        "suggest.quicksuggest.sponsored": false,
      },
      offline: {
        "quicksuggest.enabled": true,
        "quicksuggest.dataCollection.enabled": false,
        "suggest.quicksuggest.nonsponsored": true,
        "suggest.quicksuggest.sponsored": true,
      },
    };
  }

  /**
   * Prefs that are exposed in the UI and whose default-branch values are
   * configurable via Nimbus variables. This getter returns an object that maps
   * from variable names to pref names relative to `browser.urlbar`. See point 3
   * in the comment inside `_updateFirefoxSuggestScenarioHelper()` for more.
   *
   * @returns {object}
   */
  get UI_PREFS_BY_VARIABLE() {
    return {
      quickSuggestNonSponsoredEnabled: "suggest.quicksuggest.nonsponsored",
      quickSuggestSponsoredEnabled: "suggest.quicksuggest.sponsored",
      quickSuggestDataCollectionEnabled: "quicksuggest.dataCollection.enabled",
    };
  }

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

  get ONBOARDING_CHOICE() {
    return { ...ONBOARDING_CHOICE };
  }

  get ONBOARDING_URI() {
    return ONBOARDING_URI;
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
   * @returns {BlockedSuggestions}
   *   The blocked suggestions feature.
   */
  get blockedSuggestions() {
    return this.#featuresByName.get("BlockedSuggestions");
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
   */
  async init() {
    if (this.#initStarted) {
      await this.initPromise;
      return;
    }
    this.#initStarted = true;

    await this.updateFirefoxSuggestScenario();

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
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    // If any feature's enabling preference changed, update it now.
    let features = this.#featuresByEnablingPrefs.get(pref);
    if (features) {
      for (let f of features) {
        f.update();
      }
    }
  }

  /**
   * Called when a urlbar Nimbus variable changes.
   *
   * @param {string} variable
   *   The name of the variable.
   */
  async onNimbusChanged(variable) {
    if (
      variable == "quickSuggestScenario" ||
      this.UI_PREFS_BY_VARIABLE.hasOwnProperty(variable)
    ) {
      // If a change occurred to the Firefox Suggest scenario variable or any
      // variables that correspond to prefs exposed in the UI, we need to update
      // the scenario.
      await this.updateFirefoxSuggestScenario();
    } else {
      // If the current default-branch value of any pref is incorrect for the
      // intended scenario, we need to update the scenario.
      let scenario = this._getIntendedFirefoxSuggestScenario();
      let intendedDefaultPrefs = this.DEFAULT_PREFS[scenario];
      let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
      for (let [name, value] of Object.entries(intendedDefaultPrefs)) {
        // We assume all prefs are boolean right now.
        if (defaults.getBoolPref(name) != value) {
          await this.updateFirefoxSuggestScenario();
          break;
        }
      }
    }

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
   * An onboarding dialog can be shown to the users who are enrolled into
   * the Suggest experiments or rollouts. This behavior is controlled
   * by the pref `browser.urlbar.quicksuggest.shouldShowOnboardingDialog`
   * which can be remotely configured by Nimbus.
   *
   * Given that the release may overlap with another onboarding dialog, we may
   * wait for a few restarts before showing the Suggest dialog. This can
   * be remotely configured by Nimbus through
   * `quickSuggestShowOnboardingDialogAfterNRestarts`, the default is 0.
   *
   * @returns {boolean}
   *   True if the dialog was shown and false if not.
   */
  async maybeShowOnboardingDialog() {
    // The call to this method races scenario initialization on startup, and the
    // Nimbus variables we rely on below depend on the scenario, so wait for it
    // to be initialized.
    await this.initPromise;

    // If the feature is disabled, the user has already seen the dialog, or the
    // user has already opted in, don't show the onboarding.
    if (
      !lazy.UrlbarPrefs.get("quickSuggestEnabled") ||
      lazy.UrlbarPrefs.get("quicksuggest.showedOnboardingDialog") ||
      lazy.UrlbarPrefs.get("quicksuggest.dataCollection.enabled")
    ) {
      return false;
    }

    // Wait a number of restarts before showing the dialog.
    let restartsSeen = lazy.UrlbarPrefs.get("quicksuggest.seenRestarts");
    if (
      restartsSeen <
      lazy.UrlbarPrefs.get("quickSuggestShowOnboardingDialogAfterNRestarts")
    ) {
      lazy.UrlbarPrefs.set("quicksuggest.seenRestarts", restartsSeen + 1);
      return false;
    }

    let win = lazy.BrowserWindowTracker.getTopWindow();

    // Don't show the dialog on top of about:welcome for new users.
    if (win.gBrowser?.currentURI?.spec == "about:welcome") {
      return false;
    }

    if (
      !lazy.UrlbarPrefs.get("quickSuggestShouldShowOnboardingDialog") ||
      lazy.UrlbarPrefs.get("quicksuggest.contextualOptIn")
    ) {
      return false;
    }

    let variationType;
    try {
      // An error happens if the pref is not in user prefs.
      variationType = lazy.UrlbarPrefs.get(
        "quickSuggestOnboardingDialogVariation"
      ).toLowerCase();
    } catch (e) {}

    let params = { choice: undefined, variationType, visitedMain: false };
    await win.gDialogBox.open(ONBOARDING_URI, params);

    lazy.UrlbarPrefs.set("quicksuggest.showedOnboardingDialog", true);
    lazy.UrlbarPrefs.set(
      "quicksuggest.onboardingDialogVersion",
      JSON.stringify({ version: 1, variation: variationType })
    );

    // Record the user's opt-in choice on the user branch. This pref is sticky,
    // so it will retain its user-branch value regardless of what the particular
    // default was at the time.
    let optedIn = params.choice == ONBOARDING_CHOICE.ACCEPT_2;
    lazy.UrlbarPrefs.set("quicksuggest.dataCollection.enabled", optedIn);

    switch (params.choice) {
      case ONBOARDING_CHOICE.LEARN_MORE_1:
      case ONBOARDING_CHOICE.LEARN_MORE_2:
        win.openTrustedLinkIn(this.HELP_URL, "tab");
        break;
      case ONBOARDING_CHOICE.ACCEPT_2:
      case ONBOARDING_CHOICE.REJECT_2:
      case ONBOARDING_CHOICE.NOT_NOW_2:
      case ONBOARDING_CHOICE.CLOSE_1:
        // No other action required.
        break;
      default:
        params.choice = params.visitedMain
          ? ONBOARDING_CHOICE.DISMISS_2
          : ONBOARDING_CHOICE.DISMISS_1;
        break;
    }

    lazy.UrlbarPrefs.set("quicksuggest.onboardingDialogChoice", params.choice);

    return true;
  }

  /**
   * Sets the appropriate Firefox Suggest scenario based on the current Nimbus
   * rollout (if any) and "hardcoded" rollouts (if any). The possible scenarios
   * are:
   *
   * history
   *   This is the scenario when the user is not in any rollouts. Firefox
   *   Suggest suggestions are disabled.
   * offline
   *   This is the scenario for the "offline" rollout. Firefox Suggest
   *   suggestions are enabled by default. Data collection is not enabled by
   *   default, but the user can opt in in about:preferences. The onboarding
   *   dialog is not shown.
   *
   * @param {string} [testOverrides]
   *   This is intended for tests only. Pass to force the following:
   *   `{ scenario, migrationVersion, defaultPrefs, isStartup }`
   */
  async updateFirefoxSuggestScenario(testOverrides = null) {
    // Make sure we don't re-enter this method while updating prefs. Updates to
    // prefs that are fallbacks for Nimbus variables trigger the pref observer
    // in Nimbus, which triggers our Nimbus `onUpdate` callback, which calls
    // this method again.
    if (this._updatingFirefoxSuggestScenario) {
      return;
    }

    let isStartup =
      !this._updateFirefoxSuggestScenarioCalled || !!testOverrides?.isStartup;
    this._updateFirefoxSuggestScenarioCalled = true;

    try {
      this._updatingFirefoxSuggestScenario = true;

      // This is called early in startup by BrowserGlue, so make sure the user's
      // region and our Nimbus variables are initialized since the scenario may
      // depend on them. Also note that pref migrations may depend on the
      // scenario, and since each migration is performed only once, at startup,
      // prefs can end up wrong if their migrations use the wrong scenario.
      await lazy.Region.init();
      await lazy.NimbusFeatures.urlbar.ready();

      // This also races TelemetryEnvironment's initialization, so wait for it
      // to finish. TelemetryEnvironment is important because it records the
      // values of a number of Suggest preferences. If we didn't wait, we could
      // end up updating prefs after TelemetryEnvironment does its initial pref
      // cache but before it adds its observer to be notified of pref changes.
      // It would end up recording the wrong values on startup in that case.
      if (!this._testSkipTelemetryEnvironmentInit) {
        await lazy.TelemetryEnvironment.onInitialized();
      }

      this._updateFirefoxSuggestScenarioHelper(isStartup, testOverrides);
    } finally {
      this._updatingFirefoxSuggestScenario = false;
    }
  }

  _updateFirefoxSuggestScenarioHelper(isStartup, testOverrides) {
    // Updating the scenario is tricky and it's important to preserve the user's
    // choices, so we describe the process in detail below. tl;dr:
    //
    // * Prefs exposed in the UI should be sticky.
    // * Prefs that are both exposed in the UI and configurable via Nimbus
    //   should be added to `uiPrefNamesByVariable` below.
    // * Prefs that are both exposed in the UI and configurable via Nimbus don't
    //   need to be specified as a `fallbackPref` in the feature manifest.
    //   Access these prefs directly instead of through their Nimbus variables.
    // * If you are modifying this method, keep in mind that setting a pref
    //   that's a `fallbackPref` for a Nimbus variable will trigger the pref
    //   observer inside Nimbus and call all `NimbusFeatures.urlbar.onUpdate`
    //   callbacks. Inside this class we guard against that by using
    //   `_updatingFirefoxSuggestScenario`.
    //
    // The scenario-update process is described next.
    //
    // 1. Pick a scenario. If the user is in a Nimbus rollout, then Nimbus will
    //    define it. Otherwise the user may be in a "hardcoded" rollout
    //    depending on their region and locale. If the user is not in any
    //    rollouts, then the scenario is "history", which means no Firefox
    //    Suggest suggestions should appear.
    //
    // 2. Set prefs on the default branch appropriate for the scenario. We use
    //    the default branch and not the user branch because conceptually each
    //    scenario has a default behavior, which we want to distinguish from the
    //    user's choices.
    //
    //    In particular it's important to consider prefs that are exposed in the
    //    UI, like whether sponsored suggestions are enabled. Once the user
    //    makes a choice to change a default, we want to preserve that choice
    //    indefinitely regardless of the scenario the user is currently enrolled
    //    in or future scenarios they might be enrolled in. User choices are of
    //    course recorded on the user branch, so if we set scenario defaults on
    //    the user branch too, we wouldn't be able to distinguish user choices
    //    from default values. This is also why prefs that are exposed in the UI
    //    should be sticky. Unlike non-sticky prefs, sticky prefs retain their
    //    user-branch values even when those values are the same as the ones on
    //    the default branch.
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

    // 1. Pick a scenario
    let scenario =
      testOverrides?.scenario || this._getIntendedFirefoxSuggestScenario();

    // 2. Set default-branch values for the scenario
    let defaultPrefs = testOverrides?.defaultPrefs || this.DEFAULT_PREFS;
    if (!defaultPrefs.hasOwnProperty(scenario)) {
      throw new Error("No default preferences for scenario: " + scenario);
    }
    let prefs = { ...defaultPrefs[scenario] };

    // 3. Set default-branch values for prefs that are both exposed in the UI
    // and configurable via Nimbus
    for (let [variable, prefName] of Object.entries(
      this.UI_PREFS_BY_VARIABLE
    )) {
      let value = lazy.NimbusFeatures.urlbar.getVariable(variable);
      if (typeof value == "boolean") {
        prefs[prefName] = value;
      }
    }

    let defaults = Services.prefs.getDefaultBranch("browser.urlbar.");
    for (let [name, value] of Object.entries(prefs)) {
      // We assume all prefs are boolean right now.
      defaults.setBoolPref(name, value);
    }

    // 4. Migrate prefs across app versions
    if (isStartup) {
      this._ensureFirefoxSuggestPrefsMigrated(scenario, testOverrides);
    }

    // Set the scenario pref only after migrating so that migrations can tell
    // what the last-seen scenario was. Set it on the user branch so that its
    // value persists across app restarts.
    lazy.UrlbarPrefs.set("quicksuggest.scenario", scenario);
  }

  /**
   * Returns the Firefox Suggest scenario the user should be enrolled in. This
   * does *not* return the scenario they are currently enrolled in.
   *
   * @returns {string}
   *   The scenario the user should be enrolled in.
   */
  _getIntendedFirefoxSuggestScenario() {
    // If the user is in a Nimbus rollout, then Nimbus will define the scenario.
    // Otherwise the user may be in a "hardcoded" rollout depending on their
    // region and locale. If the user is not in any rollouts, then the scenario
    // is "history", which means no Firefox Suggest suggestions will appear.
    let scenario = lazy.NimbusFeatures.urlbar.getVariable(
      "quickSuggestScenario"
    );
    if (!scenario) {
      if (
        lazy.Region.home == "US" &&
        Services.locale.appLocaleAsBCP47.substring(0, 2) == "en"
      ) {
        // offline rollout for en locales in the US region
        scenario = "offline";
      } else {
        // no rollout
        scenario = "history";
      }
    }
    if (!this.DEFAULT_PREFS.hasOwnProperty(scenario)) {
      console.error(`Unrecognized Firefox Suggest scenario "${scenario}"`);
      scenario = "history";
    }
    return scenario;
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
   * @param {string} scenario
   *   The current Firefox Suggest scenario.
   * @param {string} testOverrides
   *   This is intended for tests only. Pass to force a migration version:
   *   `{ migrationVersion }`
   */
  _ensureFirefoxSuggestPrefsMigrated(scenario, testOverrides) {
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
        this[methodName](scenario);
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

  _migrateFirefoxSuggestPrefsTo_1(scenario) {
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
    if (!lazy.UrlbarPrefs.get("suggest.quicksuggest.nonsponsored")) {
      switch (scenario) {
        case "offline":
          // Set the pref on the user branch. Suggestions are enabled by default
          // for offline; we want to preserve the user's choice of opting out,
          // and we want to preserve the default-branch true value.
          lazy.UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
          break;
      }
    }
  }

  _migrateFirefoxSuggestPrefsTo_2() {
    // In previous versions of the prefs for online, suggestions were disabled
    // by default; in version 2, they're enabled by default. For users who were
    // already in online and did not enable suggestions (because they did not
    // opt in, they did opt in but later disabled suggestions, or they were not
    // shown the modal) we don't want to suddenly enable them, so if the prefs
    // do not have user-branch values, set them to false.
    if (lazy.UrlbarPrefs.get("quicksuggest.scenario") == "online") {
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

  // This is set to true when we update the Firefox Suggest scenario to prevent
  // re-entry due to pref observers. Some tests access this directly.
  _updatingFirefoxSuggestScenario = false;
}

export const QuickSuggest = new _QuickSuggest();
