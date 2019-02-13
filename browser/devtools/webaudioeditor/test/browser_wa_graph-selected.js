/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that SVG nodes and edges were created for the Graph View.
 */

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let [destId, oscId, gainId] = actors.map(actor => actor.actorID);

  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "No nodes selected on start. (destination)");
  ok(!findGraphNode(panelWin, oscId).classList.contains("selected"),
    "No nodes selected on start. (oscillator)");
  ok(!findGraphNode(panelWin, gainId).classList.contains("selected"),
    "No nodes selected on start. (gain)");

  yield clickGraphNode(panelWin, oscId);

  ok(findGraphNode(panelWin, oscId).classList.contains("selected"),
    "Selected node has class 'selected'.");
  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");
  ok(!findGraphNode(panelWin, gainId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");

  yield clickGraphNode(panelWin, gainId);

  ok(!findGraphNode(panelWin, oscId).classList.contains("selected"),
    "Previously selected node no longer has class 'selected'.");
  ok(!findGraphNode(panelWin, destId).classList.contains("selected"),
    "Non-selected nodes do not have class 'selected'.");
  ok(findGraphNode(panelWin, gainId).classList.contains("selected"),
    "Newly selected node now has class 'selected'.");

  yield teardown(target);
});
