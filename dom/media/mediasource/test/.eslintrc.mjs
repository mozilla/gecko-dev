/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default [
  {
    // Globals from mediasource.js. We use false to indicate they should not
    // be overwritten in scripts.
    languageOptions: {
      globals: {
        addMSEPrefs: false,
        fetchAndLoad: false,
        fetchAndLoadAsync: false,
        fetchWithXHR: false,
        logEvents: false,
        loadSegment: false,
        must_not_reject: false,
        must_not_throw: false,
        must_reject: false,
        must_throw: false,
        once: false,
        range: false,
        runWithMSE: false,
        wait: false,
        waitUntilTime: false,
      },
    },
    // Use const/let instead of var for tighter scoping, avoiding redeclaration
    rules: {
      "no-var": "error",
      "prefer-const": "error",
    },
  },
];
