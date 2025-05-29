/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/use-includes-instead-of-indexOf.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return {
    code,
    errors: [{ messageId: "useIncludes", type: "BinaryExpression" }],
  };
}

ruleTester.run("use-includes-instead-of-indexOf", rule, {
  valid: [
    "let a = foo.includes(bar);",
    "let a = foo.indexOf(bar) > 0;",
    "let a = foo.indexOf(bar) != 0;",
  ],
  invalid: [
    invalidCode("let a = foo.indexOf(bar) >= 0;"),
    invalidCode("let a = foo.indexOf(bar) != -1;"),
    invalidCode("let a = foo.indexOf(bar) !== -1;"),
    invalidCode("let a = foo.indexOf(bar) == -1;"),
    invalidCode("let a = foo.indexOf(bar) === -1;"),
    invalidCode("let a = foo.indexOf(bar) < 0;"),
  ],
});
