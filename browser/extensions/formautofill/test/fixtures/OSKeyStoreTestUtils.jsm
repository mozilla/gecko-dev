/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var EXPORTED_SYMBOLS = [
  "OSKeyStoreTestUtils",
];

ChromeUtils.import("resource://formautofill/OSKeyStore.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/TestUtils.jsm");

var OSKeyStoreTestUtils = {
  TEST_ONLY_REAUTH: "extensions.formautofill.osKeyStore.unofficialBuildOnlyLogin",

  setup() {
    this.ORIGINAL_STORE_LABEL = OSKeyStore.STORE_LABEL;
    OSKeyStore.STORE_LABEL = "test-" + Math.random().toString(36).substr(2);
  },

  async cleanup() {
    await OSKeyStore.cleanup();
    OSKeyStore.STORE_LABEL = this.ORIGINAL_STORE_LABEL;
  },

  /**
   * Checks whether or not the test can be run by bypassing
   * the OS login dialog. We do not want the user to be able to
   * do so with in official builds.
   * @returns {boolean} True if the test can be preformed.
   */
  canTestOSKeyStoreLogin() {
    return !AppConstants.MOZILLA_OFFICIAL;
  },

  // Wait for the observer message that simulates login success of failure.
  async waitForOSKeyStoreLogin(login = false) {
    const str = login ? "pass" : "cancel";

    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, str);

    await TestUtils.topicObserved("oskeystore-testonly-reauth",
      (subject, data) => data == str);

    Services.prefs.setStringPref(this.TEST_ONLY_REAUTH, "");
  },
};
