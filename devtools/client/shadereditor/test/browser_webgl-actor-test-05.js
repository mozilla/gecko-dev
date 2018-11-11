/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the source contents can be retrieved from the vertex and fragment
 * shader actors.
 */

function* ifWebGLSupported() {
  let { target, front } = yield initBackend(SIMPLE_CANVAS_URL);
  front.setup({ reload: true });

  let programActor = yield once(front, "program-linked");
  let vertexShader = yield programActor.getVertexShader();
  let fragmentShader = yield programActor.getFragmentShader();

  let vertSource = yield vertexShader.getText();
  ok(vertSource.includes("gl_Position"),
    "The correct vertex shader source was retrieved.");

  let fragSource = yield fragmentShader.getText();
  ok(fragSource.includes("gl_FragColor"),
    "The correct fragment shader source was retrieved.");

  yield removeTab(target.tab);
  finish();
}
