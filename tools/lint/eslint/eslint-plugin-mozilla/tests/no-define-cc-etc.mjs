/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-define-cc-etc.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, varNames) {
  if (!Array.isArray(varNames)) {
    varNames = [varNames];
  }
  return {
    code,
    errors: varNames.map(variableName => {
      return {
        messageId: "noSeparateDefinition",
        data: { name: variableName },
        type: "VariableDeclarator",
      };
    }),
  };
}

ruleTester.run("no-define-cc-etc", rule, {
  valid: [
    "var Cm = Components.manager;",
    "const CC = Components.Constructor;",
    "var {Constructor: CC, manager: Cm} = Components;",
    "const {Constructor: CC, manager: Cm} = Components;",
    "foo.Cc.test();",
    "const {bar, ...foo} = obj;",
  ],
  invalid: [
    invalidCode("var Cc;", "Cc"),
    invalidCode("let Cc;", "Cc"),
    invalidCode("let Ci;", "Ci"),
    invalidCode("let Cr;", "Cr"),
    invalidCode("let Cu;", "Cu"),
    invalidCode("var Cc = Components.classes;", "Cc"),
    invalidCode("const {classes: Cc} = Components;", "Cc"),
    invalidCode("let {classes: Cc, manager: Cm} = Components", "Cc"),
    invalidCode("const Cu = Components.utils;", "Cu"),
    invalidCode("var Ci = Components.interfaces, Cc = Components.classes;", [
      "Ci",
      "Cc",
    ]),
    invalidCode("var {'interfaces': Ci, 'classes': Cc} = Components;", [
      "Ci",
      "Cc",
    ]),
  ],
});
