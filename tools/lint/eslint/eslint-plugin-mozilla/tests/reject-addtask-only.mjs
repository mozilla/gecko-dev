/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/reject-addtask-only.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidError(output, startColumn, endColumn) {
  return [
    {
      messageId: "addTaskNotAllowed",
      column: startColumn,
      endColumn,
      line: 1,
      endLine: 1,
      suggestions: [{ messageId: "addTaskNotAllowedSuggestion", output }],
    },
  ];
}

ruleTester.run("reject-addtask-only", rule, {
  valid: [
    "add_task(foo())",
    "add_task(foo()).skip()",
    "add_task(function() {})",
    "add_task(function() {}).skip()",
  ],
  invalid: [
    {
      code: "add_task(foo()).only()",
      errors: invalidError("add_task(foo())", 16, 23),
    },
    {
      code: "add_task(foo()).only(bar())",
      errors: invalidError("add_task(foo())", 16, 28),
    },
    {
      code: "add_task(function() {}).only()",
      errors: invalidError("add_task(function() {})", 24, 31),
    },
    {
      code: "add_task(function() {}).only(bar())",
      errors: invalidError("add_task(function() {})", 24, 36),
    },
  ],
});
