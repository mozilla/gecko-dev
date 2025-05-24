/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import globals from "globals";

export default [
  {
    languageOptions: {
      // The tests in this folder are testing based on WebExtensions, so lets
      // just define the webextensions environment here.
      globals: globals.webextensions,
    },
  },
];
