/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function lineInputToCursor(line) {
  let [ line, column ] = line.split(":");
  return {
    line: line > 0 ? line - 1 : 0,
    ch: column > 0 ? column - 1 : 0 };
}

function testJumpToLine (ed, line, cursor) {
  ed.jumpToLine();
  let editorDoc = ed.container.contentDocument;
  let lineInput = editorDoc.querySelector("input");
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
  lineInput.value = line;
  lineInput.dispatchEvent(enter);
  // CodeMirror lines and columns are 0-based, Scratchpad UI is 1-based.
  let cursor = cursor || lineInputToCursor(line);
  ch(ed.getCursor(), cursor, "jumpToLine " + line + " expects cursor " + cursor.toSource());
}

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    let textLines = [
      "// line 1",
      "//  line 2",
      "//   line 3",
      "//    line 4",
      "//     line 5",
      ""];
    ed.setText(textLines.join("\n"));
    waitForFocus(function () {
      let tailpipe =
        [
          "",
          ":",
          " ",
          " : ",
          "a:b",
          "LINE:COLUMN",
          "-1",
          ":-1",
          "-1:-1",
          "0",
          ":0",
          "0:0",
        ];
      tailpipe.forEach(function (line) {
        testJumpToLine(ed, line);
      });
      textLines.map(function (line, index, object) {
        // One line beyond last newline in editor text:
        if ((object.length - index) == 1) {
          // Just jump to line
          testJumpToLine(ed, index + 1 + "", {
            line: index,
            ch: 0});
          // Jump to second character in line
          testJumpToLine(ed, (index + 1) + ":2", {
            line: index,
            ch: 0});
          // Two line beyond last newline in editor text (gets clamped):
          // Just jump to line
          testJumpToLine(ed, index + 2 + "", {
            line: index,
            ch: 0});
          // Jump to second character in line
          testJumpToLine(ed, (index + 2) + ":2", {
            line: index,
            ch: 0});
        }
        else {
          // Just jump to line
          testJumpToLine(ed, index + 1 + "");
          // Jump to second character in line
          testJumpToLine(ed, (index + 1) + ":2");
          // Jump to last character on line
          testJumpToLine(ed, (index + 1) + ":" + line.length);
          // Jump just after last character on line (end of line)
          testJumpToLine(ed, (index + 1) + ":" + (line.length + 1));
          // Jump one character past end of line (gets clamped to end of line)
          testJumpToLine(ed, (index + 1) + ":" + (line.length + 2), {
            line: index,
            ch: line.length});
        }
      });
      teardown(ed, win);
    });
  });
}
