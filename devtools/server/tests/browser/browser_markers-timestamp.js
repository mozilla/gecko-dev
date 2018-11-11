/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get a "TimeStamp" marker.
 */

const { PerformanceFront } = require("devtools/shared/fronts/performance");
const { pmmConsoleMethod, pmmLoadFrameScripts, pmmClearFrameScripts } = require("devtools/client/performance/test/helpers/profiler-mm-utils");
const MARKER_NAME = "TimeStamp";

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_perf.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = PerformanceFront(client, form);
  yield front.connect();
  let rec = yield front.startRecording({ withMarkers: true });

  pmmLoadFrameScripts(gBrowser);
  pmmConsoleMethod("timeStamp");
  pmmConsoleMethod("timeStamp", "myLabel");

  let markers = yield waitForMarkerType(front, MARKER_NAME, markers => markers.length >= 2);

  yield front.stopRecording(rec);

  ok(markers.every(({stack}) => typeof stack === "number"), "All markers have stack references.");
  ok(markers.every(({name}) => name === "TimeStamp"), "All markers found are TimeStamp markers");
  ok(markers.length === 2, "found 2 TimeStamp markers");
  ok(markers.every(({start, end}) => typeof start === "number" && start === end),
    "All markers have equal start and end times");
  is(markers[0].causeName, void 0, "Unlabeled timestamps have an empty causeName");
  is(markers[1].causeName, "myLabel", "Labeled timestamps have correct causeName");

  pmmClearFrameScripts();

  yield client.close();
  gBrowser.removeCurrentTab();
});
