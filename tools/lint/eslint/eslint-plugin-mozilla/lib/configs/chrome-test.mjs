/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parent config file for all mochitest files.

export default {
  env: {
    browser: true,
    "mozilla/browser-window": true,
  },

  // All globals made available in the test environment.
  globals: {
    // SpecialPowers is injected into the window object via SimpleTest.js
    SpecialPowers: "readonly",
    afterEach: "readonly",
    beforeEach: "readonly",
    describe: "readonly",
    extractJarToTmp: "readonly",
    getChromeDir: "readonly",
    getJar: "readonly",
    getResolvedURI: "readonly",
    getRootDirectory: "readonly",
    it: "readonly",
  },

  name: "mozilla/chrome-test",
  plugins: ["mozilla"],

  rules: {
    // We mis-predict globals for HTML test files in directories shared
    // with browser tests.
    "mozilla/no-redeclare-with-import-autofix": "off",
  },
};
