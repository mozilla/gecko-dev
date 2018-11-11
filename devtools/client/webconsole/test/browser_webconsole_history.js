/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the console history feature accessed via the up and down arrow keys.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-console.html";

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();
  hud.jsterm.clearOutput();

  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  let executeList = ["document", "window", "window.location"];

  for (let item of executeList) {
    input.value = item;
    yield jsterm.execute();
  }

  for (let x = executeList.length - 1; x != -1; x--) {
    jsterm.historyPeruse(HISTORY_BACK);
    is(input.value, executeList[x], "check history previous idx:" + x);
  }

  jsterm.historyPeruse(HISTORY_BACK);
  is(input.value, executeList[0], "test that item is still index 0");

  jsterm.historyPeruse(HISTORY_BACK);
  is(input.value, executeList[0], "test that item is still still index 0");

  for (let i = 1; i < executeList.length; i++) {
    jsterm.historyPeruse(HISTORY_FORWARD);
    is(input.value, executeList[i], "check history next idx:" + i);
  }

  jsterm.historyPeruse(HISTORY_FORWARD);
  is(input.value, "", "check input is empty again");

  // Simulate pressing Arrow_Down a few times and then if Arrow_Up shows
  // the previous item from history again.
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);
  jsterm.historyPeruse(HISTORY_FORWARD);

  is(input.value, "", "check input is still empty");

  let idxLast = executeList.length - 1;
  jsterm.historyPeruse(HISTORY_BACK);
  is(input.value, executeList[idxLast], "check history next idx:" + idxLast);
});
