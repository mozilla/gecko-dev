/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the detail views are rerendered after the range changes.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { once } = require("devtools/client/performance/test/helpers/event-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let {
    EVENTS,
    OverviewView,
    DetailsView,
    WaterfallView,
    JsCallTreeView,
    JsFlameGraphView
  } = panel.panelWin;

  let updatedWaterfall = 0;
  let updatedCallTree = 0;
  let updatedFlameGraph = 0;
  let updateWaterfall = () => updatedWaterfall++;
  let updateCallTree = () => updatedCallTree++;
  let updateFlameGraph = () => updatedFlameGraph++;
  WaterfallView.on(EVENTS.UI_WATERFALL_RENDERED, updateWaterfall);
  JsCallTreeView.on(EVENTS.UI_JS_CALL_TREE_RENDERED, updateCallTree);
  JsFlameGraphView.on(EVENTS.UI_JS_FLAMEGRAPH_RENDERED, updateFlameGraph);

  yield startRecording(panel);
  yield stopRecording(panel);

  let rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.emit(EVENTS.UI_OVERVIEW_RANGE_SELECTED, { startTime: 0, endTime: 10 });
  yield rendered;
  ok(true, "Waterfall rerenders when a range in the overview graph is selected.");

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield rendered;
  ok(true, "Flamegraph rerenders after its corresponding pane is shown.");

  rendered = once(JsFlameGraphView, EVENTS.UI_JS_FLAMEGRAPH_RENDERED);
  OverviewView.emit(EVENTS.UI_OVERVIEW_RANGE_SELECTED);
  yield rendered;
  ok(true, "Flamegraph rerenders when a range in the overview graph is removed.");

  rendered = once(JsCallTreeView, EVENTS.UI_JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;
  ok(true, "Call tree rerenders after its corresponding pane is shown.");

  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  yield DetailsView.selectView("waterfall");
  yield rendered;
  ok(true, "Waterfall rerenders after its corresponding pane is shown.");

  is(updatedWaterfall, 3, "WaterfallView rerendered 3 times.");
  is(updatedCallTree, 2, "JsCallTreeView rerendered 2 times.");
  is(updatedFlameGraph, 2, "JsFlameGraphView rerendered 2 times.");

  WaterfallView.off(EVENTS.UI_WATERFALL_RENDERED, updateWaterfall);
  JsCallTreeView.off(EVENTS.UI_JS_CALL_TREE_RENDERED, updateCallTree);
  JsFlameGraphView.off(EVENTS.UI_JS_FLAMEGRAPH_RENDERED, updateFlameGraph);

  yield teardownToolboxAndRemoveTab(panel);
});
