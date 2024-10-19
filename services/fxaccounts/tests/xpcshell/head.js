/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../common/tests/unit/head_helpers.js */
/* import-globals-from ../../../common/tests/unit/head_http.js */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
const { SCOPE_APP_SYNC, SCOPE_OLD_SYNC } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

// Some mock key data, in both scoped-key and legacy field formats.
const MOCK_ACCOUNT_KEYS = {
  scopedKeys: {
    [SCOPE_APP_SYNC]: {
      kid: "1234567890123-u7u7u7u7u7u7u7u7u7u7uw",
      k: "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqg",
      kty: "oct",
    },
  },
};

function ensureOauthConfigured() {
  Services.prefs.setBoolPref("identity.fxaccounts.oauth.enabled", true);
  Services.prefs.setStringPref(
    "identity.fxaccounts.contextParam",
    "oauth_webchannel_v1"
  );
}

function ensureOauthNotConfigured() {
  Services.prefs.setBoolPref("identity.fxaccounts.oauth.enabled", false);
  Services.prefs.setStringPref(
    "identity.fxaccounts.contextParam",
    "fx_desktop_v3"
  );
}

function resetOauthConfig() {
  Services.prefs.clearUserPref("identity.fxaccounts.oauth.enabled");
  Services.prefs.clearUserPref("identity.fxaccounts.contextParam");
}

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let { initTestLogging } = ChromeUtils.importESModule(
    "resource://testing-common/services/common/logging.sys.mjs"
  );

  initTestLogging("Trace");
}).call(this);
