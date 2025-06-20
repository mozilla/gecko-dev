/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parent config file for all mochitest files.

export default {
  env: {
    browser: true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: "readonly",
  },

  name: "mozilla/mochitest-test",
  plugins: ["mozilla"],

  rules: {
    // Turn off no-define-cc-etc for mochitests as these don't have Cc etc defined in the
    // global scope.
    "mozilla/no-define-cc-etc": "off",
    // We mis-predict globals for HTML test files in directories shared
    // with browser tests, so don't try to "fix" imports that are needed.
    "mozilla/no-redeclare-with-import-autofix": "off",
    // Turn off use-chromeutils-generateqi as these tests don't have ChromeUtils
    // available.
    "mozilla/use-chromeutils-generateqi": "off",
  },
};
