/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/valid-services.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, alias) {
  return { code, errors: [{ messageId: "unknownProperty", data: { alias } }] };
}

ruleTester.run("valid-services", rule, {
  valid: ["Services.crashmanager", "lazy.Services.crashmanager"],
  invalid: [
    invalidCode("Services.foo", "foo"),
    invalidCode("Services.foo()", "foo"),
    invalidCode("lazy.Services.foo", "foo"),
    invalidCode("Services.foo.bar()", "foo"),
    invalidCode("lazy.Services.foo.bar()", "foo"),
  ],
});
