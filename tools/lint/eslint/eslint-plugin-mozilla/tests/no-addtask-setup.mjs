/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

import rule from "../lib/rules/no-addtask-setup.mjs";
import { RuleTester } from "eslint";

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function callError() {
  return [{ messageId: "useAddSetup", type: "CallExpression" }];
}

ruleTester.run("no-addtask-setup", rule, {
  valid: [
    "add_task();",
    "add_task(function() {});",
    "add_task(function () {});",
    "add_task(function foo() {});",
    "add_task(async function() {});",
    "add_task(async function () {});",
    "add_task(async function foo() {});",
    "something(function setup() {});",
    "something(async function setup() {});",
    "add_task(setup);",
    "add_task(setup => {});",
    "add_task(async setup => {});",
  ],
  invalid: [
    {
      code: "add_task(function setup() {});",
      output: "add_setup(function() {});",
      errors: callError(),
    },
    {
      code: "add_task(function setup () {});",
      output: "add_setup(function () {});",
      errors: callError(),
    },
    {
      code: "add_task(async function setup() {});",
      output: "add_setup(async function() {});",
      errors: callError(),
    },
    {
      code: "add_task(async function setup () {});",
      output: "add_setup(async function () {});",
      errors: callError(),
    },
    {
      code: "add_task(async function setUp() {});",
      output: "add_setup(async function() {});",
      errors: callError(),
    },
    {
      code: "add_task(async function init() {});",
      output: "add_setup(async function() {});",
      errors: callError(),
    },
  ],
});
