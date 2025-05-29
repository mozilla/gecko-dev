/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-more-globals.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

function makeTest(code, errors = []) {
  return {
    code,
    errors,
    filename: import.meta.dirname + "/helper-no-more-globals.js",
  };
}

ruleTester.run("no-more-globals", rule, {
  valid: [
    makeTest("function foo() {}"),
    makeTest("var foo = 5;"),
    makeTest("let foo = 42;"),
  ],
  invalid: [
    makeTest("console.log('hello');", [
      { messageId: "removedGlobal", data: { name: "foo" } },
    ]),
    makeTest("let bar = 42;", [
      { messageId: "newGlobal", data: { name: "bar" } },
      { messageId: "removedGlobal", data: { name: "foo" } },
    ]),
  ],
});
