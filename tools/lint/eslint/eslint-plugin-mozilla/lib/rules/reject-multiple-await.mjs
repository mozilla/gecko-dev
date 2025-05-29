/**
 * @fileoverview Reject multiple await operators.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-multiple-await.html",
    },
    messages: {
      rejectMultipleAwait: "Do not use multiple await operators.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      AwaitExpression(node) {
        if (
          node.parent.type === "AwaitExpression" &&
          node.parent.parent.type !== "AwaitExpression"
        ) {
          context.report({ node, messageId: "rejectMultipleAwait" });
        }
      },
    };
  },
};
