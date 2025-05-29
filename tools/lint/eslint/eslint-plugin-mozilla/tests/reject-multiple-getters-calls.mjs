/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-multiple-getters-calls.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code) {
  return { code, errors: [{ messageId: "rejectMultipleCalls" }] };
}

ruleTester.run("reject-multiple-getters-calls", rule, {
  valid: [
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `,
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      ChromeUtils.defineESModuleGetters(window, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `,
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      if (cond) {
        ChromeUtils.defineESModuleGetters(lazy, {
          PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
        });
      }
    `,
    `
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      }, { global: "current" });
      ChromeUtils.defineESModuleGetters(lazy, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      }, { global: "shared" });
    `,
  ],
  invalid: [
    invalidCode(`
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      });
      ChromeUtils.defineESModuleGetters(lazy, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      });
    `),
    invalidCode(`
      ChromeUtils.defineESModuleGetters(lazy, {
        AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
      }, { global: "current" });
      ChromeUtils.defineESModuleGetters(lazy, {
        PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
      }, { global: "current" });
    `),
  ],
});
