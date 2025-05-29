/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-arbitrary-setTimeout.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function wrapCode(code, filename = "xpcshell/test_foo.js") {
  return { code, filename };
}

function invalidCode(code) {
  let obj = wrapCode(code);
  obj.errors = [{ messageId: "listenForEvents", type: "CallExpression" }];
  return obj;
}

ruleTester.run("no-arbitrary-setTimeout", rule, {
  valid: [
    wrapCode("setTimeout(function() {}, 0);"),
    wrapCode("setTimeout(function() {});"),
    wrapCode("setTimeout(function() {}, 10);", "test_foo.js"),
  ],
  invalid: [
    invalidCode("setTimeout(function() {}, 10);"),
    invalidCode("setTimeout(function() {}, timeout);"),
  ],
});
