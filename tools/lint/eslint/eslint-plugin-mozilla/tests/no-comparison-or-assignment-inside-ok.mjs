/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-comparison-or-assignment-inside-ok.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, output, messageId, data) {
  let rv = {
    code,
    errors: [{ messageId, data }],
  };
  if (output) {
    rv.output = output;
  }
  return rv;
}

ruleTester.run("no-comparison-or-assignment-inside-ok", rule, {
  valid: [
    "ok()",
    "ok(foo)",
    "ok(!bar)",
    "ok(!foo, 'Message')",
    "ok(bar, 'Message')",
    "ok(!!foo, 'Message')",
    // Assert.ok valid cases
    "Assert.ok()",
    "Assert.ok(foo)",
    "Assert.ok(!bar)",
    "Assert.ok(!foo, 'Message')",
    "Assert.ok(bar, 'Message')",
    "Assert.ok(!!foo, 'Message')",
  ],
  invalid: [
    // Assignment
    invalidCode("ok(foo = bar)", null, "assignment"),
    invalidCode("ok(foo = bar, 'msg')", null, "assignment"),
    invalidCode("Assert.ok(foo = bar)", null, "assignment"),
    invalidCode("Assert.ok(foo = bar, 'msg')", null, "assignment"),

    // Comparisons:
    invalidCode("ok(foo == bar)", "Assert.equal(foo, bar)", "comparison", {
      assertMethod: "Assert.equal",
      operator: "==",
    }),
    invalidCode("ok(foo != bar)", "Assert.notEqual(foo, bar)", "comparison", {
      assertMethod: "Assert.notEqual",
      operator: "!=",
    }),
    invalidCode("ok(foo < bar)", "Assert.less(foo, bar)", "comparison", {
      assertMethod: "Assert.less",
      operator: "<",
    }),
    invalidCode("ok(foo > bar)", "Assert.greater(foo, bar)", "comparison", {
      assertMethod: "Assert.greater",
      operator: ">",
    }),
    invalidCode(
      "ok(foo <= bar)",
      "Assert.lessOrEqual(foo, bar)",
      "comparison",
      { operator: "<=", assertMethod: "Assert.lessOrEqual" }
    ),
    invalidCode(
      "ok(foo >= bar)",
      "Assert.greaterOrEqual(foo, bar)",
      "comparison",
      { operator: ">=", assertMethod: "Assert.greaterOrEqual" }
    ),
    invalidCode(
      "ok(foo === bar)",
      "Assert.strictEqual(foo, bar)",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
    invalidCode(
      "ok(foo !== bar)",
      "Assert.notStrictEqual(foo, bar)",
      "comparison",
      { operator: "!==", assertMethod: "Assert.notStrictEqual" }
    ),
    // Assert.ok comparisons
    invalidCode(
      "Assert.ok(foo == bar)",
      "Assert.equal(foo, bar)",
      "comparison",
      {
        assertMethod: "Assert.equal",
        operator: "==",
      }
    ),
    invalidCode(
      "Assert.ok(foo != bar)",
      "Assert.notEqual(foo, bar)",
      "comparison",
      {
        assertMethod: "Assert.notEqual",
        operator: "!=",
      }
    ),
    invalidCode("Assert.ok(foo < bar)", "Assert.less(foo, bar)", "comparison", {
      assertMethod: "Assert.less",
      operator: "<",
    }),
    invalidCode(
      "Assert.ok(foo > bar)",
      "Assert.greater(foo, bar)",
      "comparison",
      {
        assertMethod: "Assert.greater",
        operator: ">",
      }
    ),
    invalidCode(
      "Assert.ok(foo <= bar)",
      "Assert.lessOrEqual(foo, bar)",
      "comparison",
      { operator: "<=", assertMethod: "Assert.lessOrEqual" }
    ),
    invalidCode(
      "Assert.ok(foo >= bar)",
      "Assert.greaterOrEqual(foo, bar)",
      "comparison",
      { operator: ">=", assertMethod: "Assert.greaterOrEqual" }
    ),
    invalidCode(
      "Assert.ok(foo === bar)",
      "Assert.strictEqual(foo, bar)",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
    invalidCode(
      "Assert.ok(foo !== bar)",
      "Assert.notStrictEqual(foo, bar)",
      "comparison",
      { operator: "!==", assertMethod: "Assert.notStrictEqual" }
    ),

    // Comparisons with messages:
    invalidCode(
      "ok(foo == bar, 'hi')",
      "Assert.equal(foo, bar, 'hi')",
      "comparison",
      { operator: "==", assertMethod: "Assert.equal" }
    ),
    invalidCode(
      "ok(foo != bar, 'hi')",
      "Assert.notEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!=", assertMethod: "Assert.notEqual" }
    ),
    invalidCode(
      "ok(foo < bar, 'hi')",
      "Assert.less(foo, bar, 'hi')",
      "comparison",
      { operator: "<", assertMethod: "Assert.less" }
    ),
    invalidCode(
      "ok(foo > bar, 'hi')",
      "Assert.greater(foo, bar, 'hi')",
      "comparison",
      { operator: ">", assertMethod: "Assert.greater" }
    ),
    invalidCode(
      "ok(foo <= bar, 'hi')",
      "Assert.lessOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "<=", assertMethod: "Assert.lessOrEqual" }
    ),
    invalidCode(
      "ok(foo >= bar, 'hi')",
      "Assert.greaterOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: ">=", assertMethod: "Assert.greaterOrEqual" }
    ),
    invalidCode(
      "ok(foo === bar, 'hi')",
      "Assert.strictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
    invalidCode(
      "ok(foo !== bar, 'hi')",
      "Assert.notStrictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!==", assertMethod: "Assert.notStrictEqual" }
    ),
    // Assert.ok comparisons with messages
    invalidCode(
      "Assert.ok(foo == bar, 'hi')",
      "Assert.equal(foo, bar, 'hi')",
      "comparison",
      { operator: "==", assertMethod: "Assert.equal" }
    ),
    invalidCode(
      "Assert.ok(foo != bar, 'hi')",
      "Assert.notEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!=", assertMethod: "Assert.notEqual" }
    ),
    invalidCode(
      "Assert.ok(foo < bar, 'hi')",
      "Assert.less(foo, bar, 'hi')",
      "comparison",
      { operator: "<", assertMethod: "Assert.less" }
    ),
    invalidCode(
      "Assert.ok(foo > bar, 'hi')",
      "Assert.greater(foo, bar, 'hi')",
      "comparison",
      { operator: ">", assertMethod: "Assert.greater" }
    ),
    invalidCode(
      "Assert.ok(foo <= bar, 'hi')",
      "Assert.lessOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "<=", assertMethod: "Assert.lessOrEqual" }
    ),
    invalidCode(
      "Assert.ok(foo >= bar, 'hi')",
      "Assert.greaterOrEqual(foo, bar, 'hi')",
      "comparison",
      { operator: ">=", assertMethod: "Assert.greaterOrEqual" }
    ),
    invalidCode(
      "Assert.ok(foo === bar, 'hi')",
      "Assert.strictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
    invalidCode(
      "Assert.ok(foo !== bar, 'hi')",
      "Assert.notStrictEqual(foo, bar, 'hi')",
      "comparison",
      { operator: "!==", assertMethod: "Assert.notStrictEqual" }
    ),

    // Confusing bits that break fixup:
    invalidCode(
      "async () => ok((await foo) === bar, 'Oh no')",
      "async () => Assert.strictEqual(await foo, bar, 'Oh no')",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
    invalidCode(
      "async () => Assert.ok((await foo) === bar, 'Oh no')",
      "async () => Assert.strictEqual(await foo, bar, 'Oh no')",
      "comparison",
      { operator: "===", assertMethod: "Assert.strictEqual" }
    ),
  ],
});
