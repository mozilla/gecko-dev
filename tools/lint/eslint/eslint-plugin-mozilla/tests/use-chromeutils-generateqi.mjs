/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/use-chromeutils-generateqi.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function error(messageId, type) {
  return [{ messageId, type }];
}

/* globals nsIFlug */
function QueryInterface(iid) {
  if (
    iid.equals(Ci.nsISupports) ||
    iid.equals(Ci.nsIMeh) ||
    iid.equals(nsIFlug) ||
    iid.equals(Ci.amIFoo)
  ) {
    return this;
  }
  throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
}

ruleTester.run("use-chromeutils-generateqi", rule, {
  valid: [
    `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh"]);`,
    `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh"]) }`,
  ],
  invalid: [
    {
      code: `X.prototype.QueryInterface = XPCOMUtils.generateQI(["nsIMeh"]);`,
      output: `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh"]);`,
      errors: error("noXpcomUtilsGenerateQI", "CallExpression"),
    },
    {
      code: `X.prototype = { QueryInterface: XPCOMUtils.generateQI(["nsIMeh"]) };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh"]) };`,
      errors: error("noXpcomUtilsGenerateQI", "CallExpression"),
    },
    {
      code: `X.prototype = { QueryInterface: ${QueryInterface} };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]) };`,
      errors: error("noJSQueryInterface", "Property"),
    },
    {
      code: `X.prototype = { ${String(QueryInterface).replace(
        /^function /,
        ""
      )} };`,
      output: `X.prototype = { QueryInterface: ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]) };`,
      errors: error("noJSQueryInterface", "Property"),
    },
    {
      code: `X.prototype.QueryInterface = ${QueryInterface};`,
      output: `X.prototype.QueryInterface = ChromeUtils.generateQI(["nsIMeh", "nsIFlug", "amIFoo"]);`,
      errors: error("noJSQueryInterface", "AssignmentExpression"),
    },
  ],
});
