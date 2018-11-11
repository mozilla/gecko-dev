/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the "Store as global variable" context menu item feature.
// It should be work, and be enabled only for objects

"use strict";

const TEST_URI = `data:text/html,<script>
  window.bar = { baz: 1 };
  console.log("foo");
  console.log("foo", window.bar);
</script>`;

add_task(function* () {
  yield loadTab(TEST_URI);
  let hud = yield openConsole();

  let [result] = yield waitForMessages({
    webconsole: hud,
    messages: [{
      category: CATEGORY_WEBDEV,
      severity: SEVERITY_LOG,
      count: 2,
      text: /foo/
    }],
  });

  let [msgWithText, msgWithObj] = [...result.matched];
  ok(msgWithText && msgWithObj, "Two messages should have appeared");

  let contextMenu = hud.iframeWindow.document
                                    .getElementById("output-contextmenu");
  let storeAsGlobalItem = contextMenu.querySelector("#menu_storeAsGlobal");
  let obj = msgWithObj.querySelector(".cm-variable");
  let text = msgWithText.querySelector(".console-string");
  let onceInputSet = hud.jsterm.once("set-input-value");

  info("Waiting for context menu on the object");
  yield waitForContextMenu(contextMenu, obj, () => {
    ok(storeAsGlobalItem.disabled === false, "The \"Store as global\" " +
      "context menu item should be available for objects");
    storeAsGlobalItem.click();
  }, () => {
    ok(storeAsGlobalItem.disabled === true, "The \"Store as global\" " +
      "context menu item should be disabled on popup hiding");
  });

  info("Waiting for context menu on the text node");
  yield waitForContextMenu(contextMenu, text, () => {
    ok(storeAsGlobalItem.disabled === true, "The \"Store as global\" " +
      "context menu item should be disabled for texts");
  });

  info("Waiting for input to be set");
  yield onceInputSet;

  is(hud.jsterm.getInputValue(), "temp0", "Input was set");
  let executedResult = yield hud.jsterm.execute();

  ok(executedResult.textContent.includes("{ baz: 1 }"),
     "Correct variable assigned into console");

});
