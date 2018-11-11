/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that selected nodes stay selected on graph redraw.
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = yield events;
  let [dest, osc, gain] = actors;

  yield clickGraphNode(panelWin, gain.actorID);
  ok(findGraphNode(panelWin, gain.actorID).classList.contains("selected"),
    "Node selected once.");

  // Disconnect a node to trigger a rerender
  osc.disconnect();

  yield once(panelWin, EVENTS.UI_GRAPH_RENDERED);

  ok(findGraphNode(panelWin, gain.actorID).classList.contains("selected"),
    "Node still selected after rerender.");

  yield teardown(target);
});
