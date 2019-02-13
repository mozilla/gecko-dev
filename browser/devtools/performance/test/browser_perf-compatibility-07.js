/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that recording notices does not display any buffer
 * status on servers that do not support buffer statuses.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL, void 0, {
    TEST_MOCK_PROFILER_CHECK_TIMER: 10,
    TEST_PROFILER_FILTER_STATUS: ["position", "totalSize", "generation"]
  });
  let { gFront: front, EVENTS, $, PerformanceController, PerformanceView, RecordingsView } = panel.panelWin;

  yield startRecording(panel);

  yield once(front._profiler, "profiler-status");
  ok(!$("#details-pane-container").getAttribute("buffer-status"),
    "container does not have [buffer-status] attribute when not supported");

  yield once(front._profiler, "profiler-status");
  ok(!$("#details-pane-container").getAttribute("buffer-status"),
    "container does not have [buffer-status] attribute when not supported");

  yield stopRecording(panel);

  yield teardown(panel);
  finish();
}
