/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html;charset=utf8,test Scratchpad pretty print.";
}

function runTests(sw)
{
  const sp = sw.Scratchpad;
  sp.setText([
    "// line 1",
    "// line 2",
    "var re = /a bad /regexp/; // line 3 is an obvious syntax error!",
    "// line 4",
    "// line 5",
    ""].join("\n"));
  sp.prettyPrint().then(() => {
  }).then(null, error => {
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    todo_is(/Invalid regexp flag \(3:10\)/.test(error), true, "prettyPrint expects error in editor text:\n" + error);
    let [ , errorLine, errorColumn ] = error.match(/\((\d+):(\d+)\)/);
    let editorDoc = sp.editor.container.contentDocument;
    let enter = editorDoc.createEvent("KeyboardEvent");
    enter.initKeyEvent(
      /* typeArg */ "keydown",
      !!"canBubbleArg",
      !!"cancelableArg",
      /* viewArg */ null,
      !"ctrlKeyArg",
      !"altKeyArg",
      !"shiftKeyArg",
      !"metaKeyArg",
      /* keyCodeArg */ KeyEvent.DOM_VK_RETURN,
      0);
    sp.editor.jumpToLine();
    let lineInput = editorDoc.querySelector("input");
    let errorLocation = lineInput.value;
    let [ inputLine, inputColumn ] = errorLocation.split(":");
    todo_is(errorLine, inputLine, "jumpToLine input field is set from editor selection (line)");
    todo_is(errorColumn, inputColumn, "jumpToLine input field is set from editor selection (column)");
    lineInput.dispatchEvent(enter);
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    let cursor = sp.editor.getCursor();
    todo_is(inputLine, cursor.line + 1, "jumpToLine goto error location (line)");
    todo_is(inputColumn, cursor.ch + 1, "jumpToLine goto error location (column)");
    sw.close();
    finish();
  });
}
