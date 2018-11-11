/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  BrowserTestUtils.loadURI(gBrowser, "data:text/html;charset=utf8,test Scratchpad pretty print.");
}

function runTests(sw) {
  const sp = sw.Scratchpad;
  sp.setText([
    "// line 1",
    "// line 2",
    "var re = /a bad /regexp/; // line 3 is an obvious syntax error!",
    "// line 4",
    "// line 5",
    "",
  ].join("\n"));
  sp.run().then(() => {
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    const errorLine = 3;
    const editorDoc = sp.editor.container.contentDocument;
    sp.editor.jumpToLine();
    const lineInput = editorDoc.querySelector("input");
    const inputLine = lineInput.value;
    is(inputLine, errorLine, "jumpToLine input field is set from editor selection");
    EventUtils.synthesizeKey("VK_RETURN", { }, editorDoc.defaultView);
    // CodeMirror lines and columns are 0-based, Scratchpad UI and error
    // stack are 1-based.
    const cursor = sp.editor.getCursor();
    is(cursor.line + 1, inputLine, "jumpToLine goto error location (line)");
    is(cursor.ch + 1, 1, "jumpToLine goto error location (column)");
  }, error => {
    ok(false, error);
    finish();
  }).then(() => {
    var statusBarField = sp.editor.container.ownerDocument.querySelector("#statusbar-line-col");
    const { line, ch } = sp.editor.getCursor();
    is(statusBarField.textContent, sp.strings.formatStringFromName(
      "scratchpad.statusBarLineCol", [ line + 1, ch + 1], 2), "statusbar text is correct (" + statusBarField.textContent + ")");
    finish();
  }, error => {
    ok(false, error);
    finish();
  });
}
