/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1125817
 * Tests to ensure that disconnecting a node immediately
 * after creating it does not fail.
 */

const BUG_1125817_URL = EXAMPLE_URL + "doc_bug_1125817.html";

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(BUG_1125817_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  let events = Promise.all([
    once(gAudioNodes, "add", 2),
    once(gAudioNodes, "disconnect"),
    waitForGraphRendered(panelWin, 2, 0)
  ]);
  reload(target);
  yield events;

  ok(true, "Successfully disconnected a just-created node.");

  yield teardown(target);
});
