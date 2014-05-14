/* -*- Mode: js; tab-width: 8; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    info("\ned.getText():\n" + ed.getText());
    ed.setText("// line 1\n// line 2\n/a bad /regexp/; // line 3 is an obvious syntax error!\n// line 4\n// line 5\n");
    info("\ned.getText():\n" + ed.getText());
    waitForFocus(function () {
      ed.jumpToLine();
      let editorDoc = ed.container.contentDocument;
      let lineInput = editorDoc.querySelector("input");
      let four = "4";
      let enter = "VK_RETURN";
      EventUtils.synthesizeKeyExpectEvent(four, {}, lineInput, "keypress", "Press " + four, win);
      // keypress is never seen, because keydown process the event.
      EventUtils.synthesizeKeyExpectEvent(enter, {}, lineInput, "keydown", "Press " + enter, win);
    });
    waitForFocus(function () {
      // CodeMirror lines and columns are 0-based, Scratchpad UI is 1-based.
      ch(ed.getCursor(), { line: 3, ch: 0 }, "jumpToLine({ line, ch })");
    });
    waitForFocus(function () {
      teardown(ed, win);
    });
  });
}
