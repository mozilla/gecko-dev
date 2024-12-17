/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/** @type {lazy} */
const lazy = {};
ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionPingUtils",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

const HAS_UNSUBMITTED_DATA_PREF = "captchadetection.hasUnsubmittedData";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "hasUnsubmittedData",
  HAS_UNSUBMITTED_DATA_PREF,
  false
);

const SUBMISSION_INTERVAL_PREF = "captchadetection.submissionInterval";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "submissionInterval",
  SUBMISSION_INTERVAL_PREF,
  // See #i32SafeDate() function for why we divide by 1000.
  Math.floor((24 * 60 * 60) / 1000)
);

const LAST_SUBMISSION_PREF = "captchadetection.lastSubmission";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "lastSubmission",
  LAST_SUBMISSION_PREF,
  0
);

/**
 * Utility class for handling the Captcha Detection ping.
 */
export class CaptchaDetectionPingUtils {
  static #setHasUnsubmittedDataFlag() {
    if (lazy.hasUnsubmittedData) {
      return;
    }

    Services.prefs.setBoolPref(HAS_UNSUBMITTED_DATA_PREF, true);
    CaptchaDetectionPingUtils.#setPrivacyMetrics();
  }

  static #setPrivacyMetrics() {
    lazy.console.debug("Setting privacy metrics.");
    for (const [metricName, { type, name }] of Object.entries(
      CaptchaDetectionPingUtils.prefsOfInterest
    )) {
      Glean.captchaDetection[metricName].set(
        Services.prefs["get" + type + "Pref"](name)
      );
    }
  }

  static #i32SafeDate() {
    // Prefs int values are 32-bit signed integers, so we can't store the full
    // Date.now(). We could divide by 1000, but that is safe until 2038, after
    // which it will overflow. Dividing by 1000 again will make it safe until
    // a lot longer.
    // Dates will be off by (at most) about 1000 seconds, 16.6 minutes, but that's fine.
    return Math.floor(Date.now() / 1000 / 1000);
  }

  static flushPing() {
    if (!lazy.hasUnsubmittedData) {
      lazy.console.debug("No unsubmitted data to submit.");
      return;
    }

    lazy.console.debug("Flushing ping.");
    GleanPings.captchaDetection.submit();

    lazy.console.debug("Setting unsubmitted data flag to false.");
    Services.prefs.setBoolPref(HAS_UNSUBMITTED_DATA_PREF, false);

    lazy.console.debug("Setting lastSubmission to now.");
    Services.prefs.setIntPref(LAST_SUBMISSION_PREF, this.#i32SafeDate());
  }

  static maybeSubmitPing(setHasUnsubmittedDataFlag = true) {
    if (setHasUnsubmittedDataFlag) {
      CaptchaDetectionPingUtils.#setHasUnsubmittedDataFlag();
    }

    const lastSubmission = lazy.lastSubmission;
    if (lastSubmission === 0) {
      // If this is the first time maybeSubmitPing is called, set the lastSubmission time to now
      // so that we don't submit a ping with just one event.
      lazy.console.debug("Setting lastSubmission to now.");
      Services.prefs.setIntPref(LAST_SUBMISSION_PREF, this.#i32SafeDate());
      return;
    }

    if (lastSubmission > this.#i32SafeDate() - lazy.submissionInterval) {
      lazy.console.debug("Not enough time has passed since last submission.");
      return;
    }

    CaptchaDetectionPingUtils.flushPing();
  }

  static hasPrefObservers = false;
  static prefsOfInterest = {
    networkCookieCookiebehavior: {
      type: "Int",
      name: "network.cookie.cookieBehavior",
    },
    privacyTrackingprotectionEnabled: {
      type: "Bool",
      name: "privacy.trackingprotection.enabled",
    },
    privacyTrackingprotectionCryptominingEnabled: {
      type: "Bool",
      name: "privacy.trackingprotection.cryptomining.enabled",
    },
    privacyTrackingprotectionFingerprintingEnabled: {
      type: "Bool",
      name: "privacy.trackingprotection.fingerprinting.enabled",
    },
    privacyFingerprintingprotection: {
      type: "Bool",
      name: "privacy.fingerprintingProtection",
    },
    networkCookieCookiebehaviorOptinpartitioning: {
      type: "Bool",
      name: "network.cookie.cookieBehavior.optInPartitioning",
    },
    privacyResistfingerprinting: {
      type: "Bool",
      name: "privacy.resistFingerprinting",
    },
    privacyTrackingprotectionPbmEnabled: {
      type: "Bool",
      name: "privacy.trackingprotection.pbmode.enabled",
    },
    privacyFingerprintingprotectionPbm: {
      type: "Bool",
      name: "privacy.fingerprintingProtection.pbmode",
    },
    networkCookieCookiebehaviorOptinpartitioningPbm: {
      type: "Bool",
      name: "network.cookie.cookieBehavior.optInPartitioning.pbmode",
    },
    privacyResistfingerprintingPbmode: {
      type: "Bool",
      name: "privacy.resistFingerprinting.pbmode",
    },
  };
  static init() {
    if (CaptchaDetectionPingUtils.hasPrefObservers) {
      return;
    }

    Object.values(CaptchaDetectionPingUtils.prefsOfInterest).forEach(pref => {
      Services.prefs.addObserver(
        pref.name,
        CaptchaDetectionPingUtils.flushPing
      );
    });

    ChromeUtils.idleDispatch(() =>
      CaptchaDetectionPingUtils.maybeSubmitPing(false)
    );

    this.hasPrefObservers = true;
  }
}

/**
 * @typedef lazy
 * @type {object}
 * @property {ConsoleInstance} console - console instance.
 */
