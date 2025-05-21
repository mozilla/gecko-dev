/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "sss",
  "@mozilla.org/ssservice;1",
  "nsISiteSecurityService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "certOverrideService",
  "@mozilla.org/security/certoverride;1",
  "nsICertOverrideService"
);

const CERT_PINNING_ENFORCEMENT_PREF = "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF = "network.stricttransportsecurity.preloadlist";

let requiredPreferencesSet = false;

/** @namespace */
export const Certificates = {};

/**
 * Disable all security checks and allow all certs
 * per user context or globally.
 *
 * @param {string=} userContextId
 *    Id of the user context to disable all security checks
 *    and allow all certs for it. If not provided, disable globally.
 */
Certificates.disableSecurityChecks = function (userContextId = null) {
  if (!requiredPreferencesSet) {
    requiredPreferencesSet = true;

    // Make it possible to register certificate overrides for domains that use HSTS or HPKP.
    // Disable HTTP Strict Transport Security (HSTS) preload list.
    // That means that for the websites from HSTS preload list
    // HTTPS is not going to be enforced until the website is visited.
    Services.prefs.setBoolPref(HSTS_PRELOAD_LIST_PREF, false);
    // Disable preloaded static public key pins.
    // Which means that the public key hashes of certificates
    // will not be validated against the list of static public key pins.
    Services.prefs.setIntPref(CERT_PINNING_ENFORCEMENT_PREF, 0);
  }

  if (userContextId === null) {
    lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );
  } else {
    lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
      userContextId,
      true
    );
  }
};

/**
 * Enable all security checks and allow all certs
 * per user context or globally.
 *
 * @param {string=} userContextId
 *    Id of the user context to enable all security checks
 *    and allow all certs for it. If not provided, enable globally.
 *    Note: if the security checks are enabled for a user context but disabled globally
 *    we will still have HSTS preload list and preloaded static key pins disabled
 *    for this user context.
 */
Certificates.enableSecurityChecks = function (userContextId = null) {
  if (userContextId === null) {
    lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
  } else {
    lazy.certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
      userContextId,
      false
    );
  }

  // TODO Bug 1862018. Reconsider when supporting multiple sessions.
  if (userContextId === null) {
    Services.prefs.clearUserPref(HSTS_PRELOAD_LIST_PREF);
    Services.prefs.clearUserPref(CERT_PINNING_ENFORCEMENT_PREF);

    // clear collected HSTS and HPKP state
    // through the site security service
    lazy.sss.clearAll();

    requiredPreferencesSet = false;
  }
};

/**
 * Reset security settings which were set for a user context.
 *
 * @param {string} userContextId
 *    Id of the user context to reset all security checks.
 */
Certificates.resetSecurityChecksForUserContext = function (userContextId) {
  lazy.certOverrideService.resetDisableAllSecurityChecksAndLetAttackersInterceptMyDataForUserContext(
    userContextId
  );
};
