/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that program and shader actors are cached in the frontend.
 */

async function ifWebGLSupported() {
  const { target, debuggee, panel } = await initShaderEditor(MULTIPLE_CONTEXTS_URL);
  const { EVENTS, front, shadersListView, shadersEditorsView } = panel;

  reload(target);
  const [[programActor]] = await promise.all([
    getPrograms(front, 1),
    once(panel, EVENTS.SOURCES_SHOWN),
  ]);

  const programItem = shadersListView.selectedItem;

  is(programItem.attachment.programActor, programActor,
    "The correct program actor is cached for the selected item.");

  is((await programActor.getVertexShader()),
     (await programItem.attachment.vs),
    "The cached vertex shader promise returns the correct actor.");

  is((await programActor.getFragmentShader()),
     (await programItem.attachment.fs),
    "The cached fragment shader promise returns the correct actor.");

  is((await (await programActor.getVertexShader()).getText()),
     (await (await shadersEditorsView._getEditor("vs")).getText()),
    "The cached vertex shader promise returns the correct text.");

  is((await (await programActor.getFragmentShader()).getText()),
     (await (await shadersEditorsView._getEditor("fs")).getText()),
    "The cached fragment shader promise returns the correct text.");

  await teardown(panel);
  finish();
}
