/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Create an interface to measure iterations for a micro benchmark. These iterations
 * will then be reported to the perftest runner.
 *
 * @param {string} metricName
 */
function measureIterations(metricName) {
  let accumulatedTime = 0;
  let iterations = 0;
  let now = 0;
  return {
    /**
     * Start a measurement.
     */
    start() {
      now = Cu.now();
    },
    /**
     * Stop a measurement, and record the elapsed time.
     */
    stop() {
      accumulatedTime += Cu.now() - now;
      iterations++;
    },
    /**
     * Report the metrics to perftest after finishing the microbenchmark.
     */
    reportMetrics() {
      const metrics = {};
      metrics[metricName + " iterations"] = iterations;
      metrics[metricName + " accumulatedTime"] = accumulatedTime;
      metrics[metricName + " perCallTime"] = accumulatedTime / iterations;

      info("perfMetrics", metrics);
    },
  };
}
