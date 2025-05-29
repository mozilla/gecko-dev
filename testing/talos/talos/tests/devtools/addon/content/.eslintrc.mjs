/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla from "eslint-plugin-mozilla";

export default [
  {
    plugins: { mozilla },
    languageOptions: {
      globals: {
        exports: true,
        isWorker: true,
        loader: true,
        module: true,
        reportError: true,
        require: true,
        dampWindow: true,
      },
    },
    rules: {
      "no-unused-vars": [
        "error",
        { argsIgnorePattern: "^_", caughtErrors: "none", vars: "all" },
      ],
      // These are the rules that have been configured so far to match the
      // devtools coding style.

      // Rules from the mozilla plugin
      "mozilla/no-aArgs": "error",
      "mozilla/no-define-cc-etc": "off",
      // See bug 1224289.
      "mozilla/reject-importGlobalProperties": ["error", "everything"],
      "mozilla/var-only-at-top-level": "error",
    },
  },
];
