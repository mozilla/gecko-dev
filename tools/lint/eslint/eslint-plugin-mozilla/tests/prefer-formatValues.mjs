/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/prefer-formatValues.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function error(line, column = undefined) {
  return {
    messageId: "useSingleCall",
    type: "CallExpression",
    line,
    column,
  };
}

ruleTester.run("check-length", rule, {
  valid: [
    "document.l10n.formatValue('foobar');",
    "document.l10n.formatValues(['foobar', 'foobaz']);",
    `if (foo) {
       document.l10n.formatValue('foobar');
     } else {
       document.l10n.formatValue('foobaz');
     }`,
    `document.l10n.formatValue('foobaz');
     if (foo) {
       document.l10n.formatValue('foobar');
     }`,
    `if (foo) {
       document.l10n.formatValue('foobar');
     }
     document.l10n.formatValue('foobaz');`,
    `if (foo) {
       document.l10n.formatValue('foobar');
     }
     document.l10n.formatValues(['foobaz']);`,
  ],
  invalid: [
    {
      code: `document.l10n.formatValue('foobar');
             document.l10n.formatValue('foobaz');`,
      errors: [error(1, 1), error(2, 14)],
    },
    {
      code: `document.l10n.formatValue('foobar');
             if (foo) {
               document.l10n.formatValue('foobiz');
             }
             document.l10n.formatValue('foobaz');`,
      errors: [error(1, 1), error(5, 14)],
    },
    {
      code: `document.l10n.formatValues('foobar');
             document.l10n.formatValue('foobaz');`,
      errors: [error(1, 1), error(2, 14)],
    },
  ],
});
