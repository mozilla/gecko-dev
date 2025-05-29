/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-multiple-await.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "rejectMultipleAwait" }] };
}

ruleTester.run("reject-multiple-await", rule, {
  valid: [
    "async () => await new Promise(r => r());",
    "async () => await awaitSomething;",
  ],
  invalid: [
    invalidCode("async () => await await new Promise(r => r());"),
    invalidCode(`async () => {
        await
        await new Promise(r => r());
    }`),
    invalidCode("async () => await (await new Promise(r => r()));"),
    invalidCode("async () => await await await new Promise(r => r());"),
  ],
});
