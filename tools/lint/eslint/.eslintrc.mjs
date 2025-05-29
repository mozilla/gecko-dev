/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import globals from "globals";
import eslintPlugin from "eslint-plugin-eslint-plugin";

export default [
  {
    ...eslintPlugin.configs["flat/recommended"],
    languageOptions: {
      globals: globals.node,
      parserOptions: {
        // This should match with the minimum node version that the ESLint CI
        // process uses (check the linux64-node toolchain).
        ecmaVersion: 16,
      },
    },

    rules: {
      camelcase: ["error", { properties: "never" }],
      "handle-callback-err": ["error", "er"],
      "no-undef-init": "error",
      "one-var": ["error", "never"],
      strict: ["error", "global"],
    },
  },
];
