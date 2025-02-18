/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* eslint-env node */

// Bug 1948378: remove this exception when the eslint import plugin fully
// supports exports in package.json files
// eslint-disable-next-line import/no-unresolved
import { testRule } from "stylelint-test-rule-node";
import stylelint from "stylelint";
import noBaseDesignTokens from "../rules/no-base-design-tokens.mjs";

const TEST_TOKEN_NAME = `--color-blue-60`;

let plugin = stylelint.createPlugin(
  noBaseDesignTokens.ruleName,
  noBaseDesignTokens
);
let {
  ruleName,
  rule: { messages },
} = plugin;

testRule({
  plugins: [plugin],
  ruleName,
  config: true,
  fix: false,
  accept: [
    {
      code: ".a { color: var(--color-text-link); }",
      description: "Using a semantic design token variable is valid.",
    },
    {
      code: `root { --custom-text-color-var: var(${TEST_TOKEN_NAME}); }`,
      description:
        "Assigning a design token variable to another variable is valid.",
    },
  ],
  reject: [
    {
      code: `.a { color: var(${TEST_TOKEN_NAME}); }`,
      message: messages.rejected(TEST_TOKEN_NAME),
      description: "Using a base color token directly is invalid.",
    },
  ],
});
