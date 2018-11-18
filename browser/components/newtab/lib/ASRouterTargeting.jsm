ChromeUtils.import("resource://gre/modules/components-utils/FilterExpressions.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "ASRouterPreferences",
  "resource://activity-stream/lib/ASRouterPreferences.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager",
  "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.defineModuleGetter(this, "ShellService",
  "resource:///modules/ShellService.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "AttributionCode",
  "resource:///modules/AttributionCode.jsm");

const FXA_USERNAME_PREF = "services.sync.username";
const SEARCH_REGION_PREF = "browser.search.region";
const MOZ_JEXL_FILEPATH = "mozjexl";

const {activityStreamProvider: asProvider} = NewTabUtils;

const FRECENT_SITES_UPDATE_INTERVAL = 6 * 60 * 60 * 1000; // Six hours
const FRECENT_SITES_IGNORE_BLOCKED = false;
const FRECENT_SITES_NUM_ITEMS = 25;
const FRECENT_SITES_MIN_FRECENCY = 100;

/**
 * CachedTargetingGetter
 * @param property {string} Name of the method called on ActivityStreamProvider
 * @param options {{}?} Options object passsed to ActivityStreamProvider method
 * @param updateInterval {number?} Update interval for query. Defaults to FRECENT_SITES_UPDATE_INTERVAL
 */
function CachedTargetingGetter(property, options = null, updateInterval = FRECENT_SITES_UPDATE_INTERVAL) {
  return {
    _lastUpdated: 0,
    _value: null,
    // For testing
    expire() {
      this._lastUpdated = 0;
      this._value = null;
    },
    get() {
      return new Promise(async (resolve, reject) => {
        const now = Date.now();
        if (now - this._lastUpdated >= updateInterval) {
          try {
            this._value = await asProvider[property](options);
            this._lastUpdated = now;
          } catch (e) {
            Cu.reportError(e);
            reject(e);
          }
        }
        resolve(this._value);
      });
    },
  };
}

function CheckBrowserNeedsUpdate(updateInterval = FRECENT_SITES_UPDATE_INTERVAL) {
  const UpdateChecker = Cc["@mozilla.org/updates/update-checker;1"];
  const checker = {
    _lastUpdated: 0,
    _value: null,
    // For testing. Avoid update check network call.
    setUp(value) {
      this._lastUpdated = Date.now();
      this._value = value;
    },
    expire() {
      this._lastUpdated = 0;
      this._value = null;
    },
    get() {
      return new Promise((resolve, reject) => {
        const now = Date.now();
        const updateServiceListener = {
          onCheckComplete(request, updates, updateCount) {
            checker._value = updateCount > 0;
            resolve(checker._value);
          },
          onError(request, update) {
            reject(request);
          },

          QueryInterface: ChromeUtils.generateQI(["nsIUpdateCheckListener"]),
        };

        if (UpdateChecker && (now - this._lastUpdated >= updateInterval)) {
          const checkerInstance = UpdateChecker.createInstance(Ci.nsIUpdateChecker);
          checkerInstance.checkForUpdates(updateServiceListener, true);
          this._lastUpdated = now;
        } else {
          resolve(this._value);
        }
      });
    },
  };

  return checker;
}

const QueryCache = {
  expireAll() {
    Object.keys(this.queries).forEach(query => {
      this.queries[query].expire();
    });
  },
  queries: {
    TopFrecentSites: new CachedTargetingGetter(
      "getTopFrecentSites",
      {
        ignoreBlocked: FRECENT_SITES_IGNORE_BLOCKED,
        numItems: FRECENT_SITES_NUM_ITEMS,
        topsiteFrecency: FRECENT_SITES_MIN_FRECENCY,
        onePerDomain: true,
        includeFavicon: false,
      }
    ),
    TotalBookmarksCount: new CachedTargetingGetter("getTotalBookmarksCount"),
    CheckBrowserNeedsUpdate: new CheckBrowserNeedsUpdate(),
  },
};

/**
 * sortMessagesByWeightedRank
 *
 * Each message has an associated weight, which is guaranteed to be strictly
 * positive. Sort the messages so that higher weighted messages are more likely
 * to come first.
 *
 * Specifically, sort them so that the probability of message x_1 with weight
 * w_1 appearing before message x_2 with weight w_2 is (w_1 / (w_1 + w_2)).
 *
 * This is equivalent to requiring that x_1 appearing before x_2 is (w_1 / w_2)
 * "times" as likely as x_2 appearing before x_1.
 *
 * See Bug 1484996, Comment 2 for a justification of the method.
 *
 * @param {Array} messages - A non-empty array of messages to sort, all with
 *                           strictly positive weights
 * @returns the sorted array
 */
function sortMessagesByWeightedRank(messages) {
  return messages
    .map(message => ({message, rank: Math.pow(Math.random(), 1 / message.weight)}))
    .sort((a, b) => b.rank - a.rank)
    .map(({message}) => message);
}

const TargetingGetters = {
  get locale() {
    return Services.locale.appLocaleAsLangTag;
  },
  get localeLanguageCode() {
    return Services.locale.appLocaleAsLangTag && Services.locale.appLocaleAsLangTag.substr(0, 2);
  },
  get browserSettings() {
    const {settings} = TelemetryEnvironment.currentEnvironment;
    return {
      // This way of getting attribution is deprecated - use atttributionData instead
      attribution: settings.attribution,
      update: settings.update,
    };
  },
  get attributionData() {
    // Attribution is determined at startup - so we can use the cached attribution at this point
    return AttributionCode.getCachedAttributionData();
  },
  get currentDate() {
    return new Date();
  },
  get profileAgeCreated() {
    return ProfileAge().then(times => times.created);
  },
  get profileAgeReset() {
    return ProfileAge().then(times => times.reset);
  },
  get usesFirefoxSync() {
    return Services.prefs.prefHasUserValue(FXA_USERNAME_PREF);
  },
  get sync() {
    return {
      desktopDevices: Services.prefs.getIntPref("services.sync.clients.devices.desktop", 0),
      mobileDevices: Services.prefs.getIntPref("services.sync.clients.devices.mobile", 0),
      totalDevices: Services.prefs.getIntPref("services.sync.numClients", 0),
    };
  },
  get xpinstallEnabled() {
    // This is needed for all add-on recommendations, to know if we allow xpi installs in the first place
    return Services.prefs.getBoolPref("xpinstall.enabled", true);
  },
  get addonsInfo() {
    return AddonManager.getActiveAddons(["extension", "service"])
      .then(({addons, fullData}) => {
        const info = {};
        for (const addon of addons) {
          info[addon.id] = {
            version: addon.version,
            type: addon.type,
            isSystem: addon.isSystem,
            isWebExtension: addon.isWebExtension,
          };
          if (fullData) {
            Object.assign(info[addon.id], {
              name: addon.name,
              userDisabled: addon.userDisabled,
              installDate: addon.installDate,
            });
          }
        }
        return {addons: info, isFullData: fullData};
      });
  },
  get searchEngines() {
    return new Promise(resolve => {
      // Note: calling init ensures this code is only executed after Search has been initialized
      Services.search.init(rv => {
        if (Components.isSuccessCode(rv)) {
          let engines = Services.search.getVisibleEngines();
          resolve({
            current: Services.search.defaultEngine.identifier,
            installed: engines
              .map(engine => engine.identifier)
              .filter(engine => engine),
          });
        } else {
          resolve({installed: [], current: ""});
        }
      });
    });
  },
  get isDefaultBrowser() {
    try {
      return ShellService.isDefaultBrowser();
    } catch (e) {}
    return null;
  },
  get devToolsOpenedCount() {
    return Services.prefs.getIntPref("devtools.selfxss.count");
  },
  get topFrecentSites() {
    return QueryCache.queries.TopFrecentSites.get().then(sites => sites.map(site => (
      {
        url: site.url,
        host: (new URL(site.url)).hostname,
        frecency: site.frecency,
        lastVisitDate: site.lastVisitDate,
      }
    )));
  },
  get pinnedSites() {
    return NewTabUtils.pinnedLinks.links.map(site => (site ? {
      url: site.url,
      host: (new URL(site.url)).hostname,
      searchTopSite: site.searchTopSite,
    } : {}));
  },
  get providerCohorts() {
    return ASRouterPreferences.providers.reduce((prev, current) => {
      prev[current.id] = current.cohort || "";
      return prev;
    }, {});
  },
  get totalBookmarksCount() {
    return QueryCache.queries.TotalBookmarksCount.get();
  },
  get firefoxVersion() {
    return parseInt(AppConstants.MOZ_APP_VERSION.match(/\d+/), 10);
  },
  get region() {
    return Services.prefs.getStringPref(SEARCH_REGION_PREF, "");
  },
  get needsUpdate() {
    return QueryCache.queries.CheckBrowserNeedsUpdate.get();
  },
};

this.ASRouterTargeting = {
  Environment: TargetingGetters,

  ERROR_TYPES: {
    MALFORMED_EXPRESSION: "MALFORMED_EXPRESSION",
    OTHER_ERROR: "OTHER_ERROR",
  },

  isMatch(filterExpression, customContext) {
    let context = this.Environment;
    if (customContext) {
      context = {};
      Object.defineProperties(context, Object.getOwnPropertyDescriptors(this.Environment));
      Object.defineProperties(context, Object.getOwnPropertyDescriptors(customContext));
    }

    return FilterExpressions.eval(filterExpression, context);
  },

  isTriggerMatch(trigger = {}, candidateMessageTrigger = {}) {
    if (trigger.id !== candidateMessageTrigger.id) {
      return false;
    } else if (!candidateMessageTrigger.params) {
      return true;
    }
    return candidateMessageTrigger.params.includes(trigger.param);
  },

  /**
   * checkMessageTargeting - Checks is a message's targeting parameters are satisfied
   *
   * @param {*} message An AS router message
   * @param {obj} context A FilterExpression context
   * @param {func} onError A function to handle errors (takes two params; error, message)
   * @returns
   */
  async checkMessageTargeting(message, context, onError) {
    // If no targeting is specified,
    if (!message.targeting) {
      return true;
    }
    let result;
    try {
      result = await this.isMatch(message.targeting, context);
    } catch (error) {
      Cu.reportError(error);
      if (onError) {
        const type = error.fileName.includes(MOZ_JEXL_FILEPATH) ? this.ERROR_TYPES.MALFORMED_EXPRESSION : this.ERROR_TYPES.OTHER_ERROR;
        onError(type, error, message);
      }
      result = false;
    }
    return result;
  },

  /**
   * findMatchingMessage - Given an array of messages, returns one message
   *                       whos targeting expression evaluates to true
   *
   * @param {Array} messages An array of AS router messages
   * @param {obj} impressions An object containing impressions, where keys are message ids
   * @param {trigger} string A trigger expression if a message for that trigger is desired
   * @param {obj|null} context A FilterExpression context. Defaults to TargetingGetters above.
   * @returns {obj} an AS router message
   */
  async findMatchingMessage({messages, trigger, context, onError}) {
    const sortedMessages = sortMessagesByWeightedRank([...messages]);

    for (const candidate of sortedMessages) {
      if (
        candidate &&
        (trigger ? this.isTriggerMatch(trigger, candidate.trigger) : !candidate.trigger) &&
        // If a trigger expression was passed to this function, the message should match it.
        // Otherwise, we should choose a message with no trigger property (i.e. a message that can show up at any time)
        await this.checkMessageTargeting(candidate, context, onError)
      ) {
        return candidate;
      }
    }

    return null;
  },
};

// Export for testing
this.QueryCache = QueryCache;
this.CachedTargetingGetter = CachedTargetingGetter;
this.EXPORTED_SYMBOLS = ["ASRouterTargeting", "QueryCache", "CachedTargetingGetter"];
