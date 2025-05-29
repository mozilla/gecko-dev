/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/avoid-Date-timing.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, type) {
  return { code, errors: [{ messageId: "usePerfNow", type }] };
}

ruleTester.run("avoid-Date-timing", rule, {
  valid: [
    "new Date('2017-07-11');",
    "new Date(1499790192440);",
    "new Date(2017, 7, 11);",
    "Date.UTC(2017, 7);",
  ],
  invalid: [
    invalidCode("Date.now();", "CallExpression"),
    invalidCode("new Date();", "NewExpression"),
  ],
});
