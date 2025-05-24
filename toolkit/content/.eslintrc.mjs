/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla from "eslint-plugin-mozilla";

export default [
  {
    rules: {
      // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
      // be removed & synced with the mozilla/recommended value.
      complexity: ["error", 48],
    },
  },
  {
    files: ["**/*.?(m)js"],
    ignores: ["aboutwebrtc/**"],
    languageOptions: {
      globals: mozilla.environments["browser-window"].globals,
    },
  },
];
