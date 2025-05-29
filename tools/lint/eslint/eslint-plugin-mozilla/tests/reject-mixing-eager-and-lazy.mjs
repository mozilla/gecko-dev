/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-mixing-eager-and-lazy.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, uri) {
  return { code, errors: [{ messageId: "mixedEagerAndLazy", data: { uri } }] };
}

ruleTester.run("reject-mixing-eager-and-lazy", rule, {
  valid: [
    `
    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");
`,
    `
    import{ AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
`,
    `
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
    `
    if (some_condition) {
      ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");
    }
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs"
    });
`,
  ],
  invalid: [
    invalidCode(
      `
    ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
    invalidCode(
      `
    import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
    ChromeUtils.defineESModuleGetters(lazy, {
      AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
    });
`,
      "resource://gre/modules/AppConstants.sys.mjs"
    ),
  ],
});
