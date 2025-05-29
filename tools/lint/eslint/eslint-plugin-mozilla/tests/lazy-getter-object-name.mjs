/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/lazy-getter-object-name.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return {
    code,
    errors: [{ messageId: "mustUseLazy", type: "CallExpression" }],
  };
}

ruleTester.run("lazy-getter-object-name", rule, {
  valid: [
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
  ],
  invalid: [
    invalidCode(`
    ChromeUtils.defineESModuleGetters(obj, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(this, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
    invalidCode(`
    ChromeUtils.defineESModuleGetters(window, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`),
  ],
});
