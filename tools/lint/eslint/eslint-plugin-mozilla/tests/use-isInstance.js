/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// ------------------------------------------------------------------------------
// Requirements
// ------------------------------------------------------------------------------

var rule = require("../lib/rules/use-isInstance");
const globals = require("globals");
var RuleTester = require("eslint").RuleTester;

const ruleTester = new RuleTester();

// ------------------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------------------

const errors = [
  {
    messageId: "preferIsInstance",
    type: "BinaryExpression",
  },
];

/**
 * A test case boilerplate simulating chrome privileged script.
 * @param {string} code
 */
function mockChromeScript(code) {
  return {
    code,
    filename: "foo.sys.mjs",
    languageOptions: { globals: globals.browser },
  };
}

/**
 * A test case boilerplate simulating content script.
 * @param {string} code
 */
function mockContentScript(code) {
  return {
    code,
    filename: "foo.js",
    languageOptions: { globals: globals.browser },
  };
}

ruleTester.run("use-isInstance", rule, {
  valid: [
    mockChromeScript("(() => {}) instanceof Function;"),
    mockChromeScript("({}) instanceof Object;"),
    mockChromeScript("Node instanceof Object;"),
    mockChromeScript("node instanceof lazy.Node;"),
    mockChromeScript("var Node;node instanceof Node;"),
    mockChromeScript("file instanceof lazy.File;"),
    mockChromeScript("file instanceof OS.File;"),
    mockChromeScript("file instanceof OS.File.Error;"),
    mockChromeScript("file instanceof lazy.OS.File;"),
    mockChromeScript("file instanceof lazy.OS.File.Error;"),
    mockChromeScript("file instanceof lazy.lazy.OS.File;"),
    mockChromeScript("var File;file instanceof File;"),
    mockChromeScript("foo instanceof RandomGlobalThing;"),
    mockChromeScript("foo instanceof lazy.RandomGlobalThing;"),
    mockContentScript("node instanceof Node;"),
    mockContentScript("file instanceof File;"),
    mockContentScript(
      "SpecialPowers.ChromeUtils.importESModule(''); file instanceof File;"
    ),
  ],
  invalid: [
    {
      code: "node instanceof Node",
      output: "Node.isInstance(node)",
      languageOptions: { globals: globals.browser },
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "text instanceof win.Text",
      output: "win.Text.isInstance(text)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof this.contentWindow.HTMLAudioElement",
      output: "this.contentWindow.HTMLAudioElement.isInstance(target)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof File",
      output: "File.isInstance(target)",
      languageOptions: { globals: globals.browser },
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "target instanceof win.File",
      output: "win.File.isInstance(target)",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "window.arguments[0] instanceof window.XULElement",
      output: "window.XULElement.isInstance(window.arguments[0])",
      errors,
      filename: "foo.sys.mjs",
    },
    {
      code: "ChromeUtils.importESModule(''); node instanceof Node",
      output: "ChromeUtils.importESModule(''); Node.isInstance(node)",
      languageOptions: { globals: globals.browser },
      errors,
      filename: "foo.js",
    },
    {
      code: "ChromeUtils.importESModule(''); file instanceof File",
      output: "ChromeUtils.importESModule(''); File.isInstance(file)",
      languageOptions: { globals: globals.browser },
      errors,
      filename: "foo.js",
    },
  ],
});
