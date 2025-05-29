/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/use-ownerGlobal.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return {
    code,
    errors: [{ messageId: "useOwnerGlobal", type: "MemberExpression" }],
  };
}

ruleTester.run("use-ownerGlobal", rule, {
  valid: [
    "aEvent.target.ownerGlobal;",
    "this.DOMPointNode.ownerGlobal.getSelection();",
    "windowToMessageManager(node.ownerGlobal);",
  ],
  invalid: [
    invalidCode("aEvent.target.ownerDocument.defaultView;"),
    invalidCode("this.DOMPointNode.ownerDocument.defaultView.getSelection();"),
    invalidCode("windowToMessageManager(node.ownerDocument.defaultView);"),
  ],
});
