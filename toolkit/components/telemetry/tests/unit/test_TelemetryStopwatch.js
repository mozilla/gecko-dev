/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const HIST_NAME = "TELEMETRY_SEND_SUCCESS";
const HIST_NAME2 = "RANGE_CHECKSUM_ERRORS";

var refObj = {},
  refObj2 = {};

var originalCount1, originalCount2;

function run_test() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  originalCount1 = Object.values(snapshot.values).reduce((a, b) => (a += b), 0);

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  originalCount2 = Object.values(snapshot.values).reduce((a, b) => (a += b), 0);

  Assert.ok(TelemetryStopwatch.start("mark1"));
  Assert.ok(TelemetryStopwatch.start("mark2"));

  Assert.ok(TelemetryStopwatch.start("mark1", refObj));
  Assert.ok(TelemetryStopwatch.start("mark2", refObj));

  // Same timer can't be re-started before being stopped
  Assert.ok(!TelemetryStopwatch.start("mark1"));
  Assert.ok(!TelemetryStopwatch.start("mark1", refObj));

  // Can't stop a timer that was accidentaly started twice
  Assert.ok(!TelemetryStopwatch.finish("mark1"));
  Assert.ok(!TelemetryStopwatch.finish("mark1", refObj));

  Assert.ok(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM"));
  Assert.ok(!TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM"));

  Assert.ok(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM", refObj));
  Assert.ok(!TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM", refObj));

  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2, refObj));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj2));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2, refObj2));

  Assert.ok(TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj2));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2, refObj2));

  // Verify that TS.finish deleted the timers
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, refObj));

  // Verify that they can be used again
  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj));

  Assert.ok(!TelemetryStopwatch.finish("unknown-mark")); // Unknown marker
  Assert.ok(!TelemetryStopwatch.finish("unknown-mark", {})); // Unknown object
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, {})); // Known mark on unknown object

  // Test cancel
  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.cancel(HIST_NAME));
  Assert.ok(TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that can not cancel twice
  Assert.ok(!TelemetryStopwatch.cancel(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that cancel removes the timers
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, refObj));

  finishTest();
}

function finishTest() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  let newCount = Object.values(snapshot.values).reduce((a, b) => (a += b), 0);

  Assert.equal(
    newCount - originalCount1,
    5,
    "The correct number of histograms were added for histogram 1."
  );

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  newCount = Object.values(snapshot.values).reduce((a, b) => (a += b), 0);

  Assert.equal(
    newCount - originalCount2,
    3,
    "The correct number of histograms were added for histogram 2."
  );
}
