/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-importGlobalProperties.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("reject-importGlobalProperties", rule, {
  valid: [
    {
      code: "Cu.something();",
    },
    {
      options: ["allownonwebidl"],
      code: "Cu.importGlobalProperties(['caches'])",
    },
    {
      options: ["allownonwebidl"],
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['caches'])",
    },
    {
      code: "Cu.importGlobalProperties()",
      options: ["allownonwebidl"],
    },
  ],
  invalid: [
    {
      code: "Cu.importGlobalProperties(['fetch'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['fetch'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      options: ["everything"],
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder'])",
      errors: [{ messageId: "unexpectedCallSjs" }],
      filename: "foo.sjs",
    },
    {
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder'])",
      options: ["everything"],
      errors: [{ messageId: "unexpectedCall" }],
    },
    {
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      options: ["allownonwebidl"],
      errors: [{ messageId: "unexpectedCallCuWebIdl" }],
    },
    {
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder'])",
      options: ["allownonwebidl"],
      errors: [{ messageId: "unexpectedCallXPCOMWebIdl" }],
    },
    {
      options: ["allownonwebidl"],
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      errors: [{ messageId: "unexpectedCallCuWebIdl" }],
      filename: "foo.js",
    },
    {
      options: ["allownonwebidl"],
      code: "XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder'])",
      errors: [{ messageId: "unexpectedCallXPCOMWebIdl" }],
      filename: "foo.js",
    },
    {
      options: ["allownonwebidl"],
      code: "Cu.importGlobalProperties(['TextEncoder'])",
      errors: [{ messageId: "unexpectedCallCuWebIdl" }],
      filename: "foo.sjs",
    },
  ],
});
