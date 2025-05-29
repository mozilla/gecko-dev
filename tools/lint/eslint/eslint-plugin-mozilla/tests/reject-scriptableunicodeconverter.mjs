/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-scriptableunicodeconverter.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError() {
  return [
    { messageId: "rejectScriptableUnicodeConverter", type: "MemberExpression" },
  ];
}

ruleTester.run("reject-scriptableunicodeconverter", rule, {
  valid: ["TextEncoder", "TextDecoder"],
  invalid: [
    {
      code: "Ci.nsIScriptableUnicodeConverter",
      errors: invalidError(),
    },
  ],
});
