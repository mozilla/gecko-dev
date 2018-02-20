/**
 * Bug 1369303 - A test for making sure that performance APIs have been correctly
 *   spoofed or disabled.
 */

const TEST_PATH = "http://example.net/browser/" +
                  "dom/tests/browser/";

const PERFORMANCE_TIMINGS = [
  "navigationStart",
  "unloadEventStart",
  "unloadEventEnd",
  "redirectStart",
  "redirectEnd",
  "fetchStart",
  "domainLookupStart",
  "domainLookupEnd",
  "connectStart",
  "connectEnd",
  "requestStart",
  "responseStart",
  "responseEnd",
  "domLoading",
  "domInteractive",
  "domContentLoadedEventStart",
  "domContentLoadedEventEnd",
  "domComplete",
  "loadEventStart",
  "loadEventEnd",
];

let isRounded = (x, expectedPrecision) => {
  let rounded = (Math.floor(x / expectedPrecision) * expectedPrecision);
  // First we do the perfectly normal check that should work just fine
  if (rounded === x || x === 0)
    return true;

  // When we're diving by non-whole numbers, we may not get perfect
  // multiplication/division because of floating points
  if (Math.abs(rounded - x + expectedPrecision) < .0000001) {
    return true;
  } else if (Math.abs(rounded - x) < .0000001) {
    return true;
  }

  // Then we handle the case where you're sub-millisecond and the timer is not
  // We check that the timer is not sub-millisecond by assuming it is not if it
  // returns an even number of milliseconds
  if (expectedPrecision < 1 && Math.round(x) == x) {
    if (Math.round(rounded) == x) {
      return true;
    }
  }

  ok(false, "Looming Test Failure, Additional Debugging Info: Expected Precision: " + expectedPrecision + " Measured Value: " + x +
    " Rounded Vaue: " + rounded + " Fuzzy1: " + Math.abs(rounded - x + expectedPrecision) +
    " Fuzzy 2: " + Math.abs(rounded - x));

  return false;
};

// ================================================================================================
// ================================================================================================
add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "dummy.html");

  yield ContentTask.spawn(tab.linkedBrowser, {
      list: PERFORMANCE_TIMINGS,
      precision: 2,
      isRoundedFunc: isRounded.toString()
    }, (data) => {
    let timerlist = data.list;
    let expectedPrecision = data.precision;
    // eslint beleives that isrounded is available in this scope, but if you
    // remove the assignment, you will see it is not
    // eslint-disable-next-line
    let isRounded = eval(data.isRoundedFunc);

    // Check that whether the performance timing API is correctly spoofed.
    for (let time of timerlist) {
      ok(isRounded(content.performance.timing[time], expectedPrecision), `For reduceTimerPrecision(` + expectedPrecision + `), the timing(${time}) is not correctly rounded: ` + content.performance.timing[time]);
    }

    // Try to add some entries.
    content.performance.mark("Test");
    content.performance.mark("Test-End");
    content.performance.measure("Test-Measure", "Test", "Test-End");

    // Check the entries for performance.getEntries/getEntriesByType/getEntriesByName.
    is(content.performance.getEntries().length, 3, "For reduceTimerPrecision, there should be 3 entries for performance.getEntries()");
    for (var i = 0; i < 3; i++) {
      let startTime = content.performance.getEntries()[i].startTime;
      let duration = content.performance.getEntries()[i].duration;
      ok(isRounded(startTime, expectedPrecision), "For reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i + ").startTime is not rounded: " + startTime);
      ok(isRounded(duration, expectedPrecision), "For reduceTimerPrecision(" + expectedPrecision + "), performance.getEntries(" + i + ").duration is not rounded: " + duration);
    }
    is(content.performance.getEntriesByType("mark").length, 2, "For reduceTimerPrecision, there should be 2 entries for performance.getEntriesByType()");
    is(content.performance.getEntriesByName("Test", "mark").length, 1, "For reduceTimerPrecision, there should be 1 entry for performance.getEntriesByName()");
    content.performance.clearMarks();
    content.performance.clearMeasures();
    content.performance.clearResourceTimings();
  });
  gBrowser.removeTab(tab);
});

// ================================================================================================
// ================================================================================================
add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "dummy.html");

  yield ContentTask.spawn(tab.linkedBrowser, {
    list: PERFORMANCE_TIMINGS,
    precision: 2,
    isRoundedFunc: isRounded.toString()
  }, (data) => {
    let expectedPrecision = data.precision;
    let workerCall = data.workerCall;
    return new Promise(resolve => {
      let worker = new content.Worker("file_workerPerformance.js");
      worker.onmessage = function(e) {
        if (e.data.type == "status") {
          ok(e.data.status, e.data.msg);
        } else if (e.data.type == "finish") {
          worker.terminate();
          resolve();
        } else {
          ok(false, "Unknown message type");
          worker.terminate();
          resolve();
        }
      };
    worker.postMessage({precision: expectedPrecision});
    });
  });

  gBrowser.removeTab(tab);
});
