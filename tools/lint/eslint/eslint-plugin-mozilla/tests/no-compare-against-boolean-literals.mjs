/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-compare-against-boolean-literals.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError() {
  return [{ messageId: "noCompareBoolean", type: "BinaryExpression" }];
}

ruleTester.run("no-compare-against-boolean-literals", rule, {
  valid: [`if (!foo) {}`, `if (!!foo) {}`],
  invalid: [
    {
      code: `if (foo == true) {}`,
      errors: callError(),
    },
    {
      code: `if (foo != true) {}`,
      errors: callError(),
    },
    {
      code: `if (foo == false) {}`,
      errors: callError(),
    },
    {
      code: `if (foo != false) {}`,
      errors: callError(),
    },
    {
      code: `if (true == foo) {}`,
      errors: callError(),
    },
    {
      code: `if (true != foo) {}`,
      errors: callError(),
    },
    {
      code: `if (false == foo) {}`,
      errors: callError(),
    },
    {
      code: `if (false != foo) {}`,
      errors: callError(),
    },
  ],
});
