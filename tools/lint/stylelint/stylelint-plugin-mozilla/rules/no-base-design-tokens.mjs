/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

import stylelint from "stylelint";
import { namespace } from "../helpers.mjs";

const {
  utils: { report, ruleMessages, validateOptions },
} = stylelint;

let ruleName = namespace("no-base-design-tokens");
let messages = ruleMessages(ruleName, {
  rejected: token =>
    `Avoid using the base color variable "${token}" directly; use an existing semantic token or map it to a new semantic variable instead.`,
});
let meta = {
  url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/stylelint-plugin-mozilla/rules/no-base-design-tokens.html",
};

let colorTokenRegex = /var\((?<token>--color-[a-zA-Z]+-\d+)\)/g;
let isCustomPropertyDefinition = decl => decl.prop.startsWith("--");

let ruleFunction = primaryOption => {
  return (root, result) => {
    let validOptions = validateOptions(result, ruleName, {
      actual: primaryOption,
      possible: [true],
    });

    if (!validOptions) {
      return;
    }

    root.walkDecls(decl => {
      if (isCustomPropertyDefinition(decl)) {
        return;
      }
      let tokens = [...decl.value.matchAll(colorTokenRegex)].map(
        match => match.groups?.token
      );
      if (tokens) {
        tokens.forEach(token => {
          report({
            message: messages.rejected(token),
            node: decl,
            result,
            ruleName,
          });
        });
      }
    });
  };
};

ruleFunction.ruleName = ruleName;
ruleFunction.messages = messages;
ruleFunction.meta = meta;
export default ruleFunction;
