/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/reject-multiple-await");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester({ parserOptions: { ecmaVersion: "latest" } });

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
