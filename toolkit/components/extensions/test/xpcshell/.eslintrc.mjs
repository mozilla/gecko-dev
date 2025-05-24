/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import globals from "globals";

export default [
  {
    languageOptions: {
      globals: {
        // The tests in this folder are testing based on WebExtensions, so lets
        // just define the webextensions environment here.
        ...globals.webextensions,
        // Many parts of WebExtensions test definitions (e.g. content scripts) also
        // interact with the browser environment, so define that here as we don't
        // have an easy way to handle per-function/scope usage yet.
        ...globals.browser,
      },
    },
  },
];
