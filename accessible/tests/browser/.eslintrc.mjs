/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export default [
  {
    rules: {
      "mozilla/no-aArgs": "error",
      "mozilla/reject-importGlobalProperties": ["error", "everything"],
      "mozilla/var-only-at-top-level": "error",

      "block-scoped-var": "error",
      camelcase: ["error", { properties: "never" }],
      complexity: ["error", 20],

      "handle-callback-err": ["error", "er"],
      "max-nested-callbacks": ["error", 4],
      "new-cap": ["error", { capIsNew: false }],
      "no-fallthrough": "error",
      "no-multi-str": "error",
      "no-proto": "error",
      "no-return-assign": "error",
      "no-unused-vars": [
        "error",
        { vars: "all", caughtErrors: "none", argsIgnorePattern: "^_" },
      ],
      "one-var": ["error", "never"],
      radix: "error",
      strict: ["error", "global"],
      yoda: "error",
      "no-undef-init": "error",
    },
  },
];
