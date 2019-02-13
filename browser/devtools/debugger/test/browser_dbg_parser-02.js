/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check that syntax errors are reported correctly.
 */

function test() {
  let { Parser } = Cu.import("resource:///modules/devtools/Parser.jsm", {});

  let source = "let x + 42;";
  let parser = new Parser();
  // Don't pollute the logs with exceptions that we are going to check anyhow.
  parser.logExceptions = false;
  let parsed = parser.get(source);

  ok(parsed,
    "An object should be returned even though the source had a syntax error.");

  is(parser.errors.length, 1,
    "There should be one error logged when parsing.");
  is(parser.errors[0].name, "SyntaxError",
    "The correct exception was caught.");
  is(parser.errors[0].message, "missing ; before statement",
    "The correct exception was caught.");

  finish();
}
