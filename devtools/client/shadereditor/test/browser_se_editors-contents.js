/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the editors contain the correct text when a program
 * becomes available.
 */

async function ifWebGLSupported() {
  const { target, panel } = await initShaderEditor(SIMPLE_CANVAS_URL);
  const { front, shadersEditorsView, EVENTS } = panel;

  reload(target);
  await promise.all([
    once(front, "program-linked"),
    once(panel, EVENTS.SOURCES_SHOWN),
  ]);

  const vsEditor = await shadersEditorsView._getEditor("vs");
  const fsEditor = await shadersEditorsView._getEditor("fs");

  is(vsEditor.getText().indexOf("gl_Position"), 170,
    "The vertex shader editor contains the correct text.");
  is(fsEditor.getText().indexOf("gl_FragColor"), 97,
    "The fragment shader editor contains the correct text.");

  await teardown(panel);
  finish();
}
