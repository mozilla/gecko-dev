/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that selected sheet and cursor position persists during reload.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

const LINE_NO = 5;
const COL_NO = 3;

add_task(function* () {
  let { ui } = yield openStyleEditorForURL(TESTCASE_URI);
  loadCommonFrameScript();

  is(ui.editors.length, 2, "Two sheets present after load.");

  info("Selecting the second editor");
  yield ui.selectStyleSheet(ui.editors[1].styleSheet, LINE_NO, COL_NO);

  info("Reloading page.");
  executeInContent("devtools:test:reload", {}, {}, false /* no response */);

  info("Waiting for sheets to be loaded after reload.");
  yield ui.once("stylesheets-reset");

  is(ui.editors.length, 2, "Two sheets present after reload.");

  info("Waiting for source editor to be ready.");
  yield ui.editors[1].getSourceEditor();

  is(ui.selectedEditor, ui.editors[1], "second editor is selected after reload");

  let {line, ch} = ui.selectedEditor.sourceEditor.getCursor();
  is(line, LINE_NO, "correct line selected");
  is(ch, COL_NO, "correct column selected");
});
