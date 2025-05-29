/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/rejects-requires-await.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "rejectRequiresAwait" }] };
}

ruleTester.run("reject-requires-await", rule, {
  valid: [
    "async() => { await Assert.rejects(foo, /assertion/) }",
    "async() => { await Assert.rejects(foo, /assertion/, 'msg') }",
  ],
  invalid: [
    invalidCode("Assert.rejects(foo)"),
    invalidCode("Assert.rejects(foo, 'msg')"),
  ],
});
