/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSSCompleter = require("resource://devtools/client/shared/sourceeditor/css-autocompleter.js");
const {
  cssTokenizerWithLineColumn,
} = require("resource://devtools/shared/css/parsing-utils.js");

const CSS_URI =
  "http://mochi.test:8888/browser/devtools/client/shared/sourceeditor" +
  "/test/css_statemachine_testcases.css";
const TESTS_URI =
  "http://mochi.test:8888/browser/devtools/client" +
  "/shared/sourceeditor/test/css_statemachine_tests.json";

const source = read(CSS_URI);
const { tests } = JSON.parse(read(TESTS_URI));

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent(`
    <!DOCTYPE html>
    <html>
      <head><title>CSS State machine tests.</title></head>
      <body>
        <h2>State machine tests for CSS autocompleter.</h2>
      </body>
    </html>
  `);

add_task(async function test() {
  await addTab(TEST_URI);

  const completer = new CSSCompleter({
    cssProperties: getClientCssProperties(),
  });

  let i = 0;
  for (const testcase of tests) {
    ++i;
    // if (i !== 2) continue;
    const [[line, column], expected] = testcase;
    const limitedSource = limit(source, [line, column]);

    info(`Test case ${i} from source`);
    completer.resolveState({
      source: limitedSource,
      line,
      column,
    });
    assertState(completer, expected, i + " (from_source)");

    info(`Test case ${i} from tokens`);
    completer.resolveState({
      sourceTokens: cssTokenizerWithLineColumn(limitedSource),
    });
    assertState(completer, expected, i + " (from tokens)");
  }
  gBrowser.removeCurrentTab();
});

function assertState(completer, expected, testCaseName) {
  if (checkState(completer, expected)) {
    ok(true, `Test ${testCaseName} passed. `);
  } else {
    ok(
      false,
      `Test ${testCaseName} failed. Expected state : ${JSON.stringify([
        expected[0]?.toString(),
        expected[1]?.toString(),
        expected[2],
        expected[3],
      ])} but found ${JSON.stringify([
        completer.state?.toString(),
        completer.selectorState?.toString(),
        completer.completing,
        completer.propertyName || completer.selector,
      ])}.`
    );
  }
}

function checkState(completer, expected) {
  if (
    expected[0] == "null" &&
    (!completer.state || completer.state == "null")
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[0] == "selector" &&
    expected[1] == completer.selectorState &&
    expected[2] == completer.completing &&
    expected[3] == completer.selector
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[0] == "value" &&
    expected[2] == completer.completing &&
    expected[3] == completer.propertyName
  ) {
    return true;
  } else if (
    expected[0] == completer.state &&
    expected[2] == completer.completing &&
    expected[0] != "selector" &&
    expected[0] != "value"
  ) {
    return true;
  }
  return false;
}
