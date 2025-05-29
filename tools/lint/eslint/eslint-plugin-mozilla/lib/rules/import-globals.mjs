/**
 * @fileoverview Discovers all globals for the current file.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import globals from "../globals.mjs";

export default {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/import-globals.html",
    },
    schema: [],
    type: "problem",
  },

  create: globals.getESLintGlobalParser,
};
