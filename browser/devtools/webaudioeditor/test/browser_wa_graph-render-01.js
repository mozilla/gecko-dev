/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that SVG nodes and edges were created for the Graph View.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let [destId, oscId, gainId] = actors.map(actor => actor.actorID);

  ok(findGraphNode(panelWin, oscId).classList.contains("type-OscillatorNode"), "found OscillatorNode with class");
  ok(findGraphNode(panelWin, gainId).classList.contains("type-GainNode"), "found GainNode with class");
  ok(findGraphNode(panelWin, destId).classList.contains("type-AudioDestinationNode"), "found AudioDestinationNode with class");
  is(findGraphEdge(panelWin, oscId, gainId).toString(), "[object SVGGElement]", "found edge for osc -> gain");
  is(findGraphEdge(panelWin, gainId, destId).toString(), "[object SVGGElement]", "found edge for gain -> dest");

  yield teardown(panel);
  finish();
}

