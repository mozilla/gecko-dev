/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ASRouterTargeting:
    // eslint-disable-next-line mozilla/no-browser-refs-in-toolkit
    "resource:///modules/asrouter/ASRouterTargeting.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  TargetingContext: "resource://messaging-system/targeting/Targeting.sys.mjs",
});

// Don't use ChromeUtils.defineLazyPropertyGetter because that will replace the
// property with the value upon first access, which prevents us from stubbing the ExperimentManager
// in unit tests.
Object.defineProperty(lazy, "ExperimentManager", {
  get: () => lazy.ExperimentAPI._manager,
});

const { PREF_INVALID, PREF_STRING, PREF_INT, PREF_BOOL } = Ci.nsIPrefBranch;
const PREF_TYPES = {
  [PREF_STRING]: "Ci.nsIPrefBranch.PREF_STRING",
  [PREF_INT]: "Ci.nsIPrefBranch.PREF_INT",
  [PREF_BOOL]: "Ci.nsIPrefBranch.PREF_BOOL",
};

/**
 * Return a function that returns specific keys of an object.
 *
 * All values will be awaited, so objects containing promises will be flattened
 * into objects.
 *
 * Any exceptions encountered will not prevent the key from being recorded in
 * the metric.
 *
 * @param {string[]} keys - The keys to include.
 * @returns The function.
 */
function pick(...keys) {
  const identity = x => x;
  return pickWith(Object.fromEntries(keys.map(key => [key, identity])));
}

/**
 * Return a function that returns a specific keys of an object, with transforms.
 *
 * All values will be awaited, as will their transform functions, so objects
 * containing promises will be flattened into objects.
 *
 * Any exceptions encountered will not prevent the key from being recorded in
 * the metric.
 *
 * @param {Record<string, () => any>} shape
 *        A mapping of keys to transformation functions.
 *
 * @returns The function.
 */
function pickWith(shape) {
  return async function (object) {
    const transformed = {};
    if (typeof object !== "undefined" && object !== null) {
      for (const [key, transform] of Object.entries(shape)) {
        try {
          transformed[key] = await transform(await object[key]);
        } catch (ex) {}
      }
    }
    return transformed;
  };
}

/**
 * Assert that the attribute matches the given type (via typeof).
 *
 * @param {string} expectedType
 *        The expected type.
 *        If the attribute is not of this type, this function will throw.
 * @param {any} attribute
 *        The value whose type is to be checked.
 *
 * @returns The attribute.
 */
function assertType(expectedType, attribute) {
  const type = typeof attribute;

  if (type !== expectedType) {
    throw new Error(`Expected ${expectedType} but got ${type} instead`);
  }

  return attribute;
}

/**
 * Transforms that assert that the type of the attribute matches an expected
 * type.
 */
const typeAssertions = {
  string: attribute => assertType("string", attribute),
  boolean: attribute => assertType("boolean", attribute),
  quantity: attribute => Math.floor(assertType("number", attribute)),
  array: attribute => {
    if (!Array.isArray(attribute)) {
      throw new Error(`Expected Array but got ${typeof attribute} instead`);
    }

    return attribute;
  },
  // NB: Date methods will throw if called on a non-Date object. We can't simply
  // use `attribute instanceof Date` because the Date constructor might be from
  // a different context (and thus the expression would evaluate to false).
  date: attribute => Date.prototype.toUTCString.call(attribute),
};

/**
 * This contains the set of all top-level targeting attributes in the Nimbus
 * Targeting context and optional transforms functions that will be applied
 * before the value is recorded.
 */
export const ATTRIBUTE_TRANSFORMS = Object.freeze({
  activeExperiments: typeAssertions.array,
  activeRollouts: typeAssertions.array,
  addressesSaved: typeAssertions.quantity,
  archBits: typeAssertions.quantity,
  attributionData: pick("medium", "source", "ua"),
  browserSettings: pickWith({
    update: pick("channel"),
  }),
  currentDate: typeAssertions.date,
  defaultPDFHandler: pick("knownBrowser", "registered"),
  distributionId: typeAssertions.string,
  doesAppNeedPin: typeAssertions.boolean,
  enrollmentsMap: enrollmentsMap =>
    Object.entries(enrollmentsMap).map(([experimentSlug, branchSlug]) => ({
      experimentSlug,
      branchSlug,
    })),
  firefoxVersion: typeAssertions.quantity,
  hasActiveEnterprisePolicies: typeAssertions.boolean,
  homePageSettings: pick("isCustomUrl", "isDefault", "isLocked", "isWebExt"),
  isDefaultHandler: pick("html", "pdf"),
  isDefaultBrowser: typeAssertions.boolean,
  isFirstStartup: typeAssertions.boolean,
  isFxAEnabled: typeAssertions.boolean,
  isMSIX: typeAssertions.boolean,
  memoryMB: typeAssertions.quantity,
  os: pick(
    "isLinux",
    "isMac",
    "isWindow",
    "windowsBuildNumber",
    "windowsVersion"
  ),
  profileAgeCreated: typeAssertions.quantity,
  totalBookmarksCount: typeAssertions.quantity,
  userMonthlyActivity: userMonthlyActivity =>
    userMonthlyActivity.map(([numberOfURLsVisited, date]) => ({
      numberOfURLsVisited,
      date,
    })),
  userPrefersReducedMotion: typeAssertions.boolean,
  usesFirefoxSync: typeAssertions.boolean,
  version: typeAssertions.string,
});

/**
 * Transform a targeting context attribute name to the name that Glean expects
 * for the corresponding metric.
 *
 * Glean metrics are defined in `snake_case` and are translated to `camelCase`
 * for JavaScript. Most of our targeting attributes and their Glean metric
 * equivalent have names that line up cleanly, but this falls apart when the
 * targeting attribute has a name with an all-uppercase acronym.
 *
 * For example, the metric corresponding to the `defaultPDFHandler` attribute
 * has the name `default_pdf_handler` in the metrics.yaml which would become
 * `defaultPdfhandler` in JavaScript.
 *
 * @param {string} The attribute name.
 * @returns {string} The metric name.
 */
export function normalizeAttributeName(attr) {
  switch (attr) {
    case "isFxAEnabled":
      // Would transform to `isFxAenabled";
      return attr;

    case "defaultPDFHandler":
      // Would transform to `defaultPdfhandler`.
      return "defaultPdfHandler";

    default:
      return attr.replaceAll(/[A-Z]+/g, substr => {
        return `${substr[0]}${substr.slice(1).toLowerCase()}`;
      });
  }
}

/**
 * These are the prefs that can be used in evaluation of a JEXL expression by
 * Nimbus via the `getPrefValue` filter.
 */
export const PREFS = Object.freeze({
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.addons": PREF_BOOL,
  "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features":
    PREF_BOOL,
  "browser.newtabpage.activity-stream.feeds.section.highlights": PREF_BOOL,
  "browser.newtabpage.activity-stream.feeds.section.topstories": PREF_BOOL,
  "browser.newtabpage.activity-stream.feeds.topsites": PREF_BOOL,
  "browser.newtabpage.activity-stream.showSearch": PREF_BOOL,
  "browser.newtabpage.activity-stream.showSponsoredTopSites": PREF_BOOL,
  "browser.newtabpage.enabled": PREF_BOOL,
  "browser.shopping.experience2023.autoActivateCount": PREF_INT,
  "browser.shopping.experience2023.optedIn": PREF_INT,
  "browser.toolbars.bookmarks.visibility": PREF_STRING,
  "browser.urlbar.quicksuggest.dataCollection.enabled": PREF_BOOL,
  "browser.urlbar.showSearchSuggestionsFirst": PREF_BOOL,
  "browser.urlbar.suggest.quicksuggest.sponsored": PREF_BOOL,
  "media.videocontrols.picture-in-picture.enabled": PREF_BOOL,
  "media.videocontrols.picture-in-picture.video-toggle.enabled": PREF_BOOL,
  "media.videocontrols.picture-in-picture.video-toggle.has-used": PREF_BOOL,
  "messaging-system-action.testday": PREF_STRING,
  "network.trr.mode": PREF_INT,
  "nimbus.qa.pref-1": PREF_STRING,
  "nimbus.qa.pref-2": PREF_STRING,
  "security.sandbox.content.level": PREF_INT,
  "trailhead.firstrun.didSeeAboutWelcome": PREF_BOOL,
});

/**
 * Transform a pref name to its key in the targeting context metric.
 *
 * Using dashes and periods in the object metric type would make the resulting
 * data harder to query, so we replace them with single and double underscores,
 * respectively.
 *
 * @param {string} The pref name.
 * @returns {string} The normalized pref name.
 */
export function normalizePrefName(pref) {
  return pref.replaceAll(/-/g, "_").replaceAll(/\./g, "__");
}

/**
 * Get the list of all prefs that Nimbus cares about and determine whether or
 * not they have user branch values.
 *
 * This will walk the Feature Manifest, collecting every setPref entry.
 *
 * This does not return any errors because prefHasUserValue cannot throw.
 *
 * @returns {string[]} The array of prefs.
 */
function recordUserSetPrefs() {
  const prefs = Object.values(lazy.NimbusFeatures)
    .filter(feature => feature.manifest)
    .flatMap(feature => feature.manifest.variables)
    .flatMap(Object.values)
    .filter(variable => variable.setPref)
    .map(variable => variable.setPref.pref)
    .filter(pref => Services.prefs.prefHasUserValue(pref));

  Glean.nimbusTargetingEnvironment.userSetPrefs.set(prefs);
}

/**
 * Record pref values to the nimbus_targeting_environment.pref_values metric.
 *
 * The prefs queried are determined by `PREFS`.
 *
 * Any type errors will encountered will be recorded in the
 * `nimbus_targeting_environment.pref_type_errors` metric.
 */
function recordPrefValues() {
  const prefValues = {};

  for (const [pref, expectedType] of Object.entries(PREFS)) {
    const key = normalizePrefName(pref);

    const prefType = Services.prefs.getPrefType(pref);
    if (prefType === PREF_INVALID) {
      // The pref doesn't have a value on either branch. This is not an actual
      // error.
      continue;
    }

    if (prefType !== expectedType) {
      // We cannot record this value since the pref has the wrong type.
      Glean.nimbusTargetingEnvironment.prefTypeErrors[pref].add();
      console.error(
        `TargetingContextRecorder: Pref "${pref}" has the wrong type. Expected ${PREF_TYPES[expectedType]} but found ${PREF_TYPES[prefType]}`
      );
      continue;
    }

    try {
      switch (expectedType) {
        case PREF_STRING:
          prefValues[key] = Services.prefs.getStringPref(pref);
          break;

        case PREF_INT:
          prefValues[key] = Services.prefs.getIntPref(pref);
          break;

        case PREF_BOOL:
          prefValues[key] = Services.prefs.getBoolPref(pref);
          break;
      }
    } catch (ex) {
      // `nsIPrefBranch::Get{String,Int,Bool}Pref` only fails for three reasons:
      //  - you request a pref that does not exist
      //  - you request a pref with the wrongly-typed method (e.g., you try to
      //    get the value of an int pref with `GetStringPref`)
      //  - the pref service is not available (likely because we are shutting down).
      //
      // The first two cases are covered before we attempt to read the pref
      // value and the last case is not worth recording telemetry about.
      console.error(
        `TargetingContextRecorder: Could not get value of pref "${pref}; are we shutting down?"`,
        ex
      );
    }
  }

  Glean.nimbusTargetingEnvironment.prefValues.set(prefValues);
}

/**
 * Evaluate the values of the `nimbus_targeting_context` category metrics and
 * record them.
 *
 * Any errors encountered during evaluation will be recorded in the
 * `nimbus_targeting_environment.attr_eval_errors` metric.
 *
 * The entire targeting context will be recorded inside the
 * `nimbus_targeting_environment.targeting_context_value` metric as stringified
 * JSON. The metric is disabled by default, but can be enabled via the
 * `nimbusTelemetry` feature to debug evaluation failures.
 */
async function recordTargetingContextAttributes() {
  const context = new lazy.TargetingContext(
    lazy.TargetingContext.combineContexts(
      lazy.ExperimentManager.createTargetingContext(),
      lazy.ASRouterTargeting.Environment
    )
  ).ctx;

  const recordAttrs =
    lazy.NimbusFeatures.nimbusTelemetry.getVariable(
      "nimbusTargetingEnvironment"
    )?.recordAttrs ?? null;
  const values = {};

  for (const [attr, transform] of Object.entries(ATTRIBUTE_TRANSFORMS)) {
    const metric = normalizeAttributeName(attr);
    try {
      const value = await transform(await context[attr]);

      if (recordAttrs === null || recordAttrs.includes(attr)) {
        values[metric] = value;
      }

      Glean.nimbusTargetingContext[metric].set(value);
    } catch (ex) {
      Glean.nimbusTargetingEnvironment.attrEvalErrors[metric].add();
      console.error(`TargetingContextRecorder: Could not get "${attr}"`, ex);
    }
  }

  let stringifiedCtx;
  try {
    stringifiedCtx = JSON.stringify(values);
  } catch (ex) {
    stringifiedCtx = "(JSON.stringify error)";
  }

  Glean.nimbusTargetingEnvironment.targetingContextValue.set(stringifiedCtx);
}

/**
 * Record the metrics for the nimbus-targeting-context ping and submit it.
 */
export async function recordTargetingContext() {
  recordPrefValues();
  recordUserSetPrefs();
  await recordTargetingContextAttributes();

  GleanPings.nimbusTargetingContext.submit();
}
