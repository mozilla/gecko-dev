/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-top-level-await.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "rejectTopLevelAwait" }] };
}

ruleTester.run("reject-top-level-await", rule, {
  valid: [
    "async() => { await bar() }",
    "async() => { for await (let x of []) {} }",
  ],
  invalid: [
    invalidCode("await foo"),
    invalidCode("{ await foo }"),
    invalidCode("(await foo)"),
    invalidCode("for await (let x of []) {}"),
  ],
});
