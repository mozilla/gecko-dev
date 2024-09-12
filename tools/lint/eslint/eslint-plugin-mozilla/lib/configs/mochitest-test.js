// Parent config file for all mochitest files.
"use strict";

module.exports = {
  env: {
    browser: true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: false,
  },

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
