/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import { dirname, join } from "path";

const eslintBasePath = dirname(import.meta.resolve("eslint"));

const noredeclarePath = join(eslintBasePath, "rules/no-redeclare.js");
// eslint-disable-next-line no-unsanitized/method
const baseRule = (await import(noredeclarePath)).default;
const astUtils = // eslint-disable-next-line no-unsanitized/method
  (await import(join(eslintBasePath, "rules/utils/ast-utils.js"))).default;

// Hack alert: our eslint env is pretty confused about `require` and
// `loader` for devtools modules - so ignore it for now.
// See bug 1812547
const gIgnoredImports = new Set(["loader", "require"]);

/**
 * Create a trap for a call to `report` that the original rule is
 * trying to make on `context`.
 *
 * Returns a function that forwards to `report` but provides a fixer
 * for redeclared imports that just removes those imports.
 *
 * @returns {Function}
 */
function trapReport(context) {
  return function (obj) {
    let declarator = obj.node.parent;
    while (
      declarator &&
      declarator.parent &&
      declarator.type != "VariableDeclarator"
    ) {
      declarator = declarator.parent;
    }
    if (
      declarator &&
      declarator.type == "VariableDeclarator" &&
      declarator.id.type == "ObjectPattern" &&
      declarator.init.type == "CallExpression"
    ) {
      let initialization = declarator.init;
      if (
        astUtils.isSpecificMemberAccess(
          initialization.callee,
          "ChromeUtils",
          /^importESModule$/
        )
      ) {
        // Hack alert: our eslint env is pretty confused about `require` and
        // `loader` for devtools modules - so ignore it for now.
        // See bug 1812547
        if (gIgnoredImports.has(obj.node.name)) {
          return;
        }
        // OK, we've got something we can fix. But we should be careful in case
        // there are multiple imports being destructured.
        // Do the easy (and common) case first - just one property:
        if (declarator.id.properties.length == 1) {
          context.report({
            node: declarator.parent,
            messageId: "duplicateImport",
            data: {
              name: declarator.id.properties[0].key.name,
            },
            fix(fixer) {
              return fixer.remove(declarator.parent);
            },
          });
          return;
        }

        // OK, figure out which import is duplicated here:
        let node = obj.node.parent;
        // Then remove a comma after it, or a comma before
        // if there's no comma after it.
        let sourceCode = context.sourceCode;
        let rangeToRemove = node.range;
        let tokenAfter = sourceCode.getTokenAfter(node);
        let tokenBefore = sourceCode.getTokenBefore(node);
        if (astUtils.isCommaToken(tokenAfter)) {
          rangeToRemove[1] = tokenAfter.range[1];
        } else if (astUtils.isCommaToken(tokenBefore)) {
          rangeToRemove[0] = tokenBefore.range[0];
        }
        context.report({
          node,
          messageId: "duplicateImport",
          data: {
            name: node.key.name,
          },
          fix(fixer) {
            return fixer.removeRange(rangeToRemove);
          },
        });
        return;
      }
    }
    if (context.options[0]?.errorForNonImports) {
      // Report the result from no-redeclare - we can't autofix it.
      // This can happen for other redeclaration issues, e.g. naming
      // variables in a way that conflicts with builtins like "URL" or
      // "escape".
      context.report(obj);
    }
  };
}

export default {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-redeclare-with-import-autofix.html",
    },
    messages: {
      ...baseRule.meta.messages,
      duplicateImport:
        "The import of '{{ name }}' is redundant with one set up earlier (e.g. head.js or the browser window environment). It should be removed.",
    },
    schema: [
      {
        type: "object",
        properties: {
          errorForNonImports: {
            type: "boolean",
            default: true,
          },
        },
        additionalProperties: false,
      },
    ],
    type: "suggestion",
    fixable: "code",
  },

  create(context) {
    let newOptions = [{ builtinGlobals: true }];
    const contextForBaseRule = Object.create(context, {
      report: {
        value: trapReport(context),
        writable: false,
      },
      options: {
        value: newOptions,
      },
    });
    return baseRule.create(contextForBaseRule);
  },
};
