/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that line graphs hide the tooltips when there's no data available.

const TEST_DATA = [];
let LineGraphWidget = devtools.require("devtools/shared/widgets/LineGraphWidget");
let {Promise} = devtools.require("resource://gre/modules/Promise.jsm");

add_task(function*() {
  yield promiseTab("about:blank");
  yield performTest();
  gBrowser.removeCurrentTab();
});

function* performTest() {
  let [host, win, doc] = yield createHost();
  let graph = new LineGraphWidget(doc.body, "fps");

  yield testGraph(graph);

  yield graph.destroy();
  host.destroy();
}

function* testGraph(graph) {
  yield graph.setDataWhenReady(TEST_DATA);

  is(graph._gutter.hidden, true,
    "The gutter should be hidden, since there's no data available.");
  is(graph._maxTooltip.hidden, true,
    "The max tooltip should be hidden.");
  is(graph._avgTooltip.hidden, true,
    "The avg tooltip should be hidden.");
  is(graph._minTooltip.hidden, true,
    "The min tooltip should be hidden.");
}
