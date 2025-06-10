/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREFS_CHANGING_CATEGORY = new Set([
  "network.cookie.cookieBehavior",
  "network.cookie.cookieBehavior.pbmode",
  "network.http.referer.disallowCrossSiteRelaxingDefault",
  "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation",
  "privacy.partition.network_state.ocsp_cache",
  "privacy.query_stripping.enabled",
  "privacy.query_stripping.enabled.pbmode",
  "privacy.fingerprintingProtection",
  "privacy.fingerprintingProtection.pbmode",
]);

/**
 * @class ContentBlockingPrefs
 *
 * Manages how the content blocking and anti-tracking preferences relate to the
 * broad Tracking Protection categories (standard, strict and custom).
 *
 * @typedef {"standard"|"strict"|"custom"} CBCategory
 */
export let ContentBlockingPrefs = {
  PREF_CB_CATEGORY: "browser.contentblocking.category",
  PREF_STRICT_DEF: "browser.contentblocking.features.strict",
  switchingCategory: false,

  setPrefExpectations() {
    // The prefs inside CATEGORY_PREFS are initial values.
    // If the pref remains null, then it will expect the default value.
    // The "standard" category is defined as expecting default values of the
    // listed prefs. The "strict" category lists all prefs that will be set
    // according to the strict feature pref.
    this.CATEGORY_PREFS = {
      strict: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.trackingprotection.emailtracking.enabled": null,
        "privacy.trackingprotection.emailtracking.pbmode.enabled": null,
        "privacy.trackingprotection.consentmanager.skip.enabled": null,
        "privacy.trackingprotection.consentmanager.skip.pbmode.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation":
          null,
        "privacy.partition.network_state.ocsp_cache": null,
        "privacy.query_stripping.enabled": null,
        "privacy.query_stripping.enabled.pbmode": null,
        "privacy.fingerprintingProtection": null,
        "privacy.fingerprintingProtection.pbmode": null,
        "network.cookie.cookieBehavior.optInPartitioning": null,
        "privacy.bounceTrackingProtection.mode": null,
      },
      standard: {
        "network.cookie.cookieBehavior": null,
        "network.cookie.cookieBehavior.pbmode": null,
        "privacy.trackingprotection.pbmode.enabled": null,
        "privacy.trackingprotection.enabled": null,
        "privacy.trackingprotection.socialtracking.enabled": null,
        "privacy.trackingprotection.fingerprinting.enabled": null,
        "privacy.trackingprotection.cryptomining.enabled": null,
        "privacy.trackingprotection.emailtracking.enabled": null,
        "privacy.trackingprotection.emailtracking.pbmode.enabled": null,
        "privacy.trackingprotection.consentmanager.skip.enabled": null,
        "privacy.trackingprotection.consentmanager.skip.pbmode.enabled": null,
        "privacy.annotate_channels.strict_list.enabled": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault": null,
        "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation":
          null,
        "privacy.partition.network_state.ocsp_cache": null,
        "privacy.query_stripping.enabled": null,
        "privacy.query_stripping.enabled.pbmode": null,
        "privacy.fingerprintingProtection": null,
        "privacy.fingerprintingProtection.pbmode": null,
        "network.cookie.cookieBehavior.optInPartitioning": null,
        "privacy.bounceTrackingProtection.mode": null,
      },
    };
    let type = "strict";
    let rulesArray = Services.prefs
      .getStringPref(this.PREF_STRICT_DEF)
      .split(",");
    for (let item of rulesArray) {
      switch (item) {
        case "tp":
          this.CATEGORY_PREFS[type]["privacy.trackingprotection.enabled"] =
            true;
          break;
        case "-tp":
          this.CATEGORY_PREFS[type]["privacy.trackingprotection.enabled"] =
            false;
          break;
        case "tpPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.pbmode.enabled"
          ] = true;
          break;
        case "-tpPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.pbmode.enabled"
          ] = false;
          break;
        case "fp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = true;
          break;
        case "-fp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.fingerprinting.enabled"
          ] = false;
          break;
        case "cryptoTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = true;
          break;
        case "-cryptoTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.cryptomining.enabled"
          ] = false;
          break;
        case "stp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = true;
          break;
        case "-stp":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.socialtracking.enabled"
          ] = false;
          break;
        case "emailTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.enabled"
          ] = true;
          break;
        case "-emailTP":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.enabled"
          ] = false;
          break;
        case "emailTPPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.pbmode.enabled"
          ] = true;
          break;
        case "-emailTPPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.emailtracking.pbmode.enabled"
          ] = false;
          break;
        case "consentmanagerSkip":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.consentmanager.skip.enabled"
          ] = true;
          break;
        case "-consentmanagerSkip":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.consentmanager.skip.enabled"
          ] = false;
          break;
        case "consentmanagerSkipPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.consentmanager.skip.pbmode.enabled"
          ] = true;
          break;
        case "-consentmanagerSkipPrivate":
          this.CATEGORY_PREFS[type][
            "privacy.trackingprotection.consentmanager.skip.pbmode.enabled"
          ] = false;
          break;
        case "lvl2":
          this.CATEGORY_PREFS[type][
            "privacy.annotate_channels.strict_list.enabled"
          ] = true;
          break;
        case "-lvl2":
          this.CATEGORY_PREFS[type][
            "privacy.annotate_channels.strict_list.enabled"
          ] = false;
          break;
        case "rp":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault"
          ] = true;
          break;
        case "-rp":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault"
          ] = false;
          break;
        case "rpTop":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation"
          ] = true;
          break;
        case "-rpTop":
          this.CATEGORY_PREFS[type][
            "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation"
          ] = false;
          break;
        case "ocsp":
          this.CATEGORY_PREFS[type][
            "privacy.partition.network_state.ocsp_cache"
          ] = true;
          break;
        case "-ocsp":
          this.CATEGORY_PREFS[type][
            "privacy.partition.network_state.ocsp_cache"
          ] = false;
          break;
        case "qps":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled"] = true;
          break;
        case "-qps":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled"] = false;
          break;
        case "qpsPBM":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled.pbmode"] =
            true;
          break;
        case "-qpsPBM":
          this.CATEGORY_PREFS[type]["privacy.query_stripping.enabled.pbmode"] =
            false;
          break;
        case "fpp":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection"] = true;
          break;
        case "-fpp":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection"] = false;
          break;
        case "fppPrivate":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection.pbmode"] =
            true;
          break;
        case "-fppPrivate":
          this.CATEGORY_PREFS[type]["privacy.fingerprintingProtection.pbmode"] =
            false;
          break;
        case "cookieBehavior0":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehavior1":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehavior2":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehavior3":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehavior4":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehavior5":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        case "cookieBehaviorPBM0":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_ACCEPT;
          break;
        case "cookieBehaviorPBM1":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
          break;
        case "cookieBehaviorPBM2":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT;
          break;
        case "cookieBehaviorPBM3":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
          break;
        case "cookieBehaviorPBM4":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;
          break;
        case "cookieBehaviorPBM5":
          this.CATEGORY_PREFS[type]["network.cookie.cookieBehavior.pbmode"] =
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
          break;
        case "3pcd":
          this.CATEGORY_PREFS[type][
            "network.cookie.cookieBehavior.optInPartitioning"
          ] = true;
          break;
        case "-3pcd":
          this.CATEGORY_PREFS[type][
            "network.cookie.cookieBehavior.optInPartitioning"
          ] = false;
          break;
        case "btp":
          this.CATEGORY_PREFS[type]["privacy.bounceTrackingProtection.mode"] =
            Ci.nsIBounceTrackingProtection.MODE_ENABLED;
          break;
        case "-btp":
          // We currently consider MODE_ENABLED_DRY_RUN the "off" state. See
          // nsIBounceTrackingProtection.idl for details.
          this.CATEGORY_PREFS[type]["privacy.bounceTrackingProtection.mode"] =
            Ci.nsIBounceTrackingProtection.MODE_ENABLED_DRY_RUN;
          break;
        default:
          console.error(`Error: Unknown rule observed ${item}`);
      }
    }
  },

  /**
   * Checks if CB prefs match perfectly with one of our pre-defined categories.
   *
   * @param {CBCategory} category
   */
  prefsMatch(category) {
    // The category pref must be either unset, or match.
    if (
      Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY) &&
      Services.prefs.getStringPref(this.PREF_CB_CATEGORY) != category
    ) {
      return false;
    }
    for (let pref in this.CATEGORY_PREFS[category]) {
      let value = this.CATEGORY_PREFS[category][pref];
      if (value == null) {
        if (Services.prefs.prefHasUserValue(pref)) {
          return false;
        }
      } else {
        let prefType = Services.prefs.getPrefType(pref);
        if (
          (prefType == Services.prefs.PREF_BOOL &&
            Services.prefs.getBoolPref(pref) != value) ||
          (prefType == Services.prefs.PREF_INT &&
            Services.prefs.getIntPref(pref) != value) ||
          (prefType == Services.prefs.PREF_STRING &&
            Services.prefs.getStringPref(pref) != value)
        ) {
          return false;
        }
      }
    }
    return true;
  },

  matchCBCategory() {
    if (this.switchingCategory) {
      return;
    }
    // If PREF_CB_CATEGORY is not set match users to a Content Blocking category. Check if prefs fit
    // perfectly into strict or standard, otherwise match with custom. If PREF_CB_CATEGORY has previously been set,
    // a change of one of these prefs necessarily puts us in "custom".
    if (this.prefsMatch("standard")) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "standard");
    } else if (this.prefsMatch("strict")) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "strict");
    } else {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "custom");
    }

    // If there is a custom policy which changes a related pref, then put the user in custom so
    // they still have access to other content blocking prefs, and to keep our default definitions
    // from changing.
    let policy = Services.policies.getActivePolicies();
    if (
      policy &&
      ((policy.EnableTrackingProtection &&
        !policy.EnableTrackingProtection.Category) ||
        policy.Cookies)
    ) {
      Services.prefs.setStringPref(this.PREF_CB_CATEGORY, "custom");
    }
  },

  updateCBCategory() {
    if (
      this.switchingCategory ||
      !Services.prefs.prefHasUserValue(this.PREF_CB_CATEGORY)
    ) {
      return;
    }
    // Turn on switchingCategory flag, to ensure that when the individual prefs that change as a result
    // of the category change do not trigger yet another category change.
    this.switchingCategory = true;
    let value = Services.prefs.getStringPref(this.PREF_CB_CATEGORY);
    this.setPrefsToCategory(value);
    this.switchingCategory = false;
  },

  /**
   * Sets all user-exposed content blocking preferences to values that match the selected category.
   *
   * @param {CBCategory} category
   */
  setPrefsToCategory(category, lockPrefs) {
    // Leave prefs as they were if we are switching to "custom" category.
    if (category == "custom") {
      return;
    }

    for (let pref in this.CATEGORY_PREFS[category]) {
      let value = this.CATEGORY_PREFS[category][pref];
      if (!Services.prefs.prefIsLocked(pref)) {
        if (value == null) {
          Services.prefs.clearUserPref(pref);
        } else {
          switch (Services.prefs.getPrefType(pref)) {
            case Services.prefs.PREF_BOOL:
              Services.prefs.setBoolPref(pref, value);
              break;
            case Services.prefs.PREF_INT:
              Services.prefs.setIntPref(pref, value);
              break;
            case Services.prefs.PREF_STRING:
              Services.prefs.setStringPref(pref, value);
              break;
          }
          if (lockPrefs) {
            Services.prefs.lockPref(pref);
          }
        }
      }
    }
  },

  setPrefExpectationsAndUpdate() {
    this.setPrefExpectations();
    this.updateCBCategory();
  },

  observe(subject, topic, data) {
    if (topic != "nsPref:changed") {
      return;
    }

    if (
      data.startsWith("privacy.trackingprotection") ||
      PREFS_CHANGING_CATEGORY.has(data)
    ) {
      this.matchCBCategory();
    }

    if (data.startsWith("privacy.trackingprotection")) {
      this.setPrefExpectations();
    } else if (data == this.PREF_CB_CATEGORY) {
      this.updateCBCategory();
    } else if (data == "browser.contentblocking.features.strict") {
      this.setPrefExpectationsAndUpdate();
    }
  },

  init() {
    this.setPrefExpectationsAndUpdate();
    this.matchCBCategory();

    for (let prefix of PREF_PREFIXES_TO_OBSERVE) {
      Services.prefs.addObserver(prefix, this);
    }
  },

  uninit() {
    for (let prefix of PREF_PREFIXES_TO_OBSERVE) {
      Services.prefs.removeObserver(prefix, this);
    }
  },
};

const PREF_PREFIXES_TO_OBSERVE = new Set([
  "privacy.trackingprotection",
  "network.cookie.cookieBehavior",
  "network.http.referer.disallowCrossSiteRelaxingDefault",
  "privacy.partition.network_state.ocsp_cache",
  "privacy.query_stripping.enabled",
  "privacy.fingerprintingProtection",
  ContentBlockingPrefs.PREF_CB_CATEGORY,
  ContentBlockingPrefs.PREF_STRICT_DEF,
]);

ContentBlockingPrefs.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);
