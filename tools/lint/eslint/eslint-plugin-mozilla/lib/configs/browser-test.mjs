/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Parent config file for all browser-chrome files.

export default {
  env: {
    browser: true,
    "mozilla/browser-window": true,
    "mozilla/simpletest": true,
  },

  // All globals made available in the test environment.
  globals: {
    // `$` is defined in SimpleTest.js
    $: false,
    Assert: false,
    BrowserTestUtils: false,
    ContentTask: false,
    EventUtils: false,
    IOUtils: false,
    PathUtils: false,
    PromiseDebugging: false,
    SpecialPowers: false,
    TestUtils: false,
    addLoadEvent: false,
    add_setup: false,
    add_task: false,
    afterEach: false,
    beforeEach: false,
    content: false,
    describe: false,
    executeSoon: false,
    expectUncaughtException: false,
    export_assertions: false,
    extractJarToTmp: false,
    finish: false,
    gTestPath: false,
    getChromeDir: false,
    getJar: false,
    getResolvedURI: false,
    getRootDirectory: false,
    getTestFilePath: false,
    ignoreAllUncaughtExceptions: false,
    info: false,
    is: false,
    isnot: false,
    it: false,
    ok: false,
    record: false,
    registerCleanupFunction: false,
    requestLongerTimeout: false,
    setExpectedFailuresForSelfTest: false,
    stringContains: false,
    stringMatches: false,
    todo: false,
    todo_is: false,
    todo_isnot: false,
    waitForClipboard: false,
    waitForExplicitFinish: false,
    waitForFocus: false,
  },

  name: "mozilla/browser-test",
  plugins: ["mozilla"],

  rules: {
    "mozilla/no-addtask-setup": "error",
    "mozilla/no-comparison-or-assignment-inside-ok": "error",
    "mozilla/no-redeclare-with-import-autofix": [
      "error",
      { errorForNonImports: false },
    ],
  },
};
