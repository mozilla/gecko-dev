/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import globals from "globals";
import mozilla from "eslint-plugin-mozilla";

export default [
  {
    languageOptions: {
      globals: {
        ...globals.webextensions,
        getTestConfig: false,
        startMark: true,
        endMark: true,
        name: true,
      },
    },
    plugins: { mozilla },
    rules: {
      "mozilla/avoid-Date-timing": "error",
    },
  },
];
