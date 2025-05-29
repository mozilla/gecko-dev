/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-eager-module-in-lazy-getter.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, uri) {
  return { code, errors: [{ messageId: "eagerModule", data: { uri } }] };
}

ruleTester.run("reject-eager-module-in-lazy-getter", rule, {
  valid: [
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      Integration: "resource://gre/modules/Integration.sys.mjs",
    });
`,
  ],
  invalid: [
    invalidCode(
      `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
  ],
});
