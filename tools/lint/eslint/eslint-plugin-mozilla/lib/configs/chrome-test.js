// Parent config file for all mochitest files.
"use strict";

module.exports = {
  env: {
    browser: true,
    "mozilla/browser-window": true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: false,
    extractJarToTmp: false,
    getChromeDir: false,
    getJar: false,
    getResolvedURI: false,
    getRootDirectory: false,
  },

  plugins: ["mozilla"],

  rules: {
    // We mis-predict globals for HTML test files in directories shared
    // with browser tests.
    "mozilla/no-redeclare-with-import-autofix": "off",
  },
};
