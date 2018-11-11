/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the graphs' selections are linked.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { UI_ENABLE_MEMORY_PREF } = require("devtools/client/performance/test/helpers/prefs");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");
const { startRecording, stopRecording } = require("devtools/client/performance/test/helpers/actions");
const { times } = require("devtools/client/performance/test/helpers/event-utils");
const { dragStartCanvasGraph, dragStopCanvasGraph } = require("devtools/client/performance/test/helpers/input-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { EVENTS, OverviewView } = panel.panelWin;

  // Enable memory to test.
  Services.prefs.setBoolPref(UI_ENABLE_MEMORY_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  let markersOverview = OverviewView.graphs.get("timeline");
  let memoryGraph = OverviewView.graphs.get("memory");
  let framerateGraph = OverviewView.graphs.get("framerate");
  let width = framerateGraph.width;

  // Perform a selection inside the framerate graph.

  let rangeSelected = times(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, 2);
  dragStartCanvasGraph(framerateGraph, { x: 0 });
  dragStopCanvasGraph(framerateGraph, { x: width / 2 });
  yield rangeSelected;

  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryGraph.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");
  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (width / 2) + "})",
    "The framerate graph has a correct selection.");

  // Perform a selection inside the markers overview.

  markersOverview.dropSelection();

  rangeSelected = times(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, 2);
  dragStartCanvasGraph(markersOverview, { x: 0 });
  dragStopCanvasGraph(markersOverview, { x: width / 4 });
  yield rangeSelected;

  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryGraph.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");
  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (width / 4) + "})",
    "The framerate graph has a correct selection.");

  // Perform a selection inside the memory overview.

  markersOverview.dropSelection();

  rangeSelected = times(OverviewView, EVENTS.UI_OVERVIEW_RANGE_SELECTED, 2);
  dragStartCanvasGraph(memoryGraph, { x: 0 });
  dragStopCanvasGraph(memoryGraph, { x: width / 10 });
  yield rangeSelected;

  is(markersOverview.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The markers overview has a correct selection.");
  is(memoryGraph.getSelection().toSource(), framerateGraph.getSelection().toSource(),
    "The memory overview has a correct selection.");
  is(framerateGraph.getSelection().toSource(), "({start:0, end:" + (width / 10) + "})",
    "The framerate graph has a correct selection.");

  yield teardownToolboxAndRemoveTab(panel);
});
