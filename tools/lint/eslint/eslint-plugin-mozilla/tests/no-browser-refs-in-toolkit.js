/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/no-browser-refs-in-toolkit");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

function invalidCode(code, url) {
  return {
    code,
    errors: [
      {
        messageId: "noBrowserChrome",
        data: { url },
        type: "Literal",
      },
    ],
  };
}

ruleTester.run("no-browser-refs-in-toolkit", rule, {
  valid: [
    'import foo from "chrome://global/content/aboutAbout.html"',
    'ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs")',
    'ChromeUtils.defineESModuleGetters(null, {foo: "toolkit/global/aboutFoo.ftl"})',
    'import foo from "moz-src:///toolkit/Foo.sys.mjs"',
    'import foo from "moz-src:///toolkit/browser.js"',
    'import foo from "moz-src://bar/toolkit/browser.js"',
  ],
  invalid: [
    invalidCode(
      'import foo from "chrome://browser/content/browser.xhtml"',
      "chrome://browser/content/browser.xhtml"
    ),
    invalidCode(
      'ChromeUtils.importESModule("resource:///modules/BrowserWindowTracker.sys.mjs")',
      "resource:///modules/BrowserWindowTracker.sys.mjs"
    ),
    invalidCode(
      'ChromeUtils.defineESModuleGetters(null, {foo: "browser/browser.ftl"})',
      "browser/browser.ftl"
    ),
    invalidCode(
      'import foo from "moz-src:///browser/Foo.sys.mjs"',
      "moz-src:///browser/Foo.sys.mjs"
    ),
    invalidCode(
      'import foo from "moz-src://bar/browser/Foo.sys.mjs"',
      "moz-src://bar/browser/Foo.sys.mjs"
    ),
  ],
});
