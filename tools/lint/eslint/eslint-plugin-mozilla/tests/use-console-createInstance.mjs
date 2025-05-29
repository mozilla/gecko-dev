/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/use-console-createInstance.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("use-console-createInstance", rule, {
  valid: ['"resource://gre/modules/Foo.sys.mjs"'],
  invalid: [
    {
      code: '"resource://gre/modules/Console.sys.mjs"',
      errors: [
        {
          messageId: "useConsoleRatherThanModule",
          data: { module: "Console.sys.mjs" },
        },
      ],
    },
    {
      code: '"resource://gre/modules/Log.sys.mjs"',
      errors: [
        {
          messageId: "useConsoleRatherThanModule",
          data: { module: "Log.sys.mjs" },
        },
      ],
    },
  ],
});
