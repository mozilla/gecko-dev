/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-import-system-module-from-non-system.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

ruleTester.run("reject-import-system-module-from-non-system", rule, {
  valid: [
    {
      code: `const { AppConstants } = ChromeUtils.importESM("resource://gre/modules/AppConstants.sys.mjs");`,
    },
  ],
  invalid: [
    {
      code: `import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";`,
      errors: [{ messageId: "rejectStaticImportSystemModuleFromNonSystem" }],
    },
  ],
});
