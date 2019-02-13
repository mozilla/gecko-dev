ok(self.performance, "Performance object should exist.");
ok(typeof self.performance.now == 'function', "Performance object should have a 'now' method.");
var n = self.performance.now(), d = Date.now();
ok(n >= 0, "The value of now() should be equal to or greater than 0.");
ok(self.performance.now() >= n, "The value of now() should monotonically increase.");

// The spec says performance.now() should have micro-second resolution, but allows 1ms if the platform doesn't support it.
// Our implementation does provide micro-second resolution, except for windows XP combined with some HW properties
// where we can't use QueryPerformanceCounters (see comments at mozilla-central/xpcom/ds/TimeStamp_windows.cpp).
// This XP-low-res case results in about 15ms resolutions, and can be identified when perf.now() returns only integers.
//
// Since setTimeout might return too early/late, our goal is that perf.now() changed within 2ms
// (or 25ms for XP-low-res), rather than specific number of setTimeout(N) invocations.
// See bug 749894 (intermittent failures of this test)
var platformPossiblyLowRes;
workerTestGetOSCPU(function(oscpu) {
    platformPossiblyLowRes = oscpu.indexOf("Windows NT 5.1") == 0; // XP only
    setTimeout(checkAfterTimeout, 1);
});
var allInts = (n % 1) == 0; // Indicator of limited HW resolution.
var checks = 0;

function checkAfterTimeout() {
  checks++;
  var d2 = Date.now();
  var n2 = self.performance.now();

  allInts = allInts && (n2 % 1) == 0;
  var lowResCounter = platformPossiblyLowRes && allInts;

  if ( n2 == n && checks < 50 && // 50 is just a failsafe. Our real goals are 2ms or 25ms.
       ( (d2 - d) < 2 // The spec allows 1ms resolution. We allow up to measured 2ms to ellapse.
         ||
         lowResCounter &&
         (d2 - d) < 25
       )
     ) {
    setTimeout(checkAfterTimeout, 1);
    return;
  }

  // Loose spec: 1ms resolution, or 15ms resolution for the XP-low-res case.
  // We shouldn't test that dt is actually within 2/25ms since the iterations break if it isn't, and timeout could be late.
  ok(n2 > n, "Loose - the value of now() should increase within 2ms (or 25ms if low-res counter) (delta now(): " + (n2 - n) + " ms).");

  // Strict spec: if it's not the XP-low-res case, while the spec allows 1ms resolution, it prefers microseconds, which we provide.
  // Since the fastest setTimeout return which I observed was ~500 microseconds, a microseconds counter should change in 1 iteretion.
  ok(n2 > n && (lowResCounter || checks == 1),
     "Strict - [if high-res counter] the value of now() should increase after one setTimeout (hi-res: " + (!lowResCounter) +
                                                                                              ", iters: " + checks +
                                                                                              ", dt: " + (d2 - d) +
                                                                                              ", now(): " + n2 + ").");
  workerTestDone();
};
