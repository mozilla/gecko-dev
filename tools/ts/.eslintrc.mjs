/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import eslintPlugin from "eslint-plugin-eslint-plugin";
import globals from "globals";

export default [
  {
    plugins: { "eslint-plugin": eslintPlugin },
    ...eslintPlugin.configs["flat/recommended"],
    languageOptions: {
      globals: { ...globals.browser, ...globals.node },
    },
    rules: {
      "no-console": "off",
      strict: ["error", "global"],
    },
  },
];
