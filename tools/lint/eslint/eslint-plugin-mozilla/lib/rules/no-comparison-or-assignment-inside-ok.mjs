/**
 * @file Don't allow accidental assignments inside `ok()`,
 *               and encourage people to use appropriate alternatives
 *               when using comparisons between 2 values.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const operatorToAssertionMap = {
  "==": "Assert.equal",
  "===": "Assert.strictEqual",
  "!=": "Assert.notEqual",
  "!==": "Assert.notStrictEqual",
  ">": "Assert.greater",
  "<": "Assert.less",
  "<=": "Assert.lessOrEqual",
  ">=": "Assert.greaterOrEqual",
};

export default {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-comparison-or-assignment-inside-ok.html",
    },
    fixable: "code",
    messages: {
      assignment:
        "Assigning to a variable inside ok() is odd - did you mean to compare the two?",
      comparison:
        "Use dedicated assertion methods ({{assertMethod}}) rather than ok(a {{operator}} b).",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    const exprs = new Set(["BinaryExpression", "AssignmentExpression"]);
    return {
      CallExpression(node) {
        // Support both ok() and Assert.ok()
        let isOk =
          (node.callee.type === "Identifier" && node.callee.name === "ok") ||
          (node.callee.type === "MemberExpression" &&
            node.callee.object.type === "Identifier" &&
            node.callee.object.name === "Assert" &&
            node.callee.property.type === "Identifier" &&
            node.callee.property.name === "ok");
        if (!isOk) {
          return;
        }
        let firstArg = node.arguments[0];
        if (!firstArg || !exprs.has(firstArg.type)) {
          return;
        }
        if (firstArg.type == "AssignmentExpression") {
          context.report({
            node: firstArg,
            messageId: "assignment",
          });
        } else if (
          firstArg.type == "BinaryExpression" &&
          operatorToAssertionMap.hasOwnProperty(firstArg.operator)
        ) {
          context.report({
            node,
            messageId: "comparison",
            data: {
              assertMethod: operatorToAssertionMap[firstArg.operator],
              operator: firstArg.operator,
            },
            fix: fixer => {
              let left = context.sourceCode.getText(firstArg.left);
              let right = context.sourceCode.getText(firstArg.right);
              let message =
                node.arguments.length > 1
                  ? ", " + context.sourceCode.getText(node.arguments[1])
                  : "";
              // Replace the whole call with the correct Assert method
              let assertion = `${operatorToAssertionMap[firstArg.operator]}(${left}, ${right}${message})`;
              return fixer.replaceText(node, assertion);
            },
          });
        }
      },
    };
  },
};
