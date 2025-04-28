/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported timeThis */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Load common.js and promisified-events.js from accessible/tests/mochitest/ for
// all tests.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

// All the A11Y metrics in tools/performance/PerfStats.h.
const ALL_A11Y_PERFSTATS =
  (1 << 29) |
  (1 << 30) |
  (1 << 31) |
  (1 << 32) |
  (1 << 33) |
  (1 << 34) |
  (1 << 35) |
  (1 << 36) |
  (1 << 37) |
  (1 << 38) |
  (1 << 39) |
  (1 << 40) |
  (1 << 41) |
  (1 << 42) |
  (1 << 43) |
  (1 << 44);

/**
 * Time a function and log how long it took. The given name is included in log
 * messages. All accessibility PerfStats metrics are also captured and logged.
 */
async function timeThis(name, func) {
  const logPrefix = `Timing: ${name}`;
  info(`${logPrefix}: begin`);
  const start = performance.now();
  ChromeUtils.setPerfStatsCollectionMask(ALL_A11Y_PERFSTATS);
  await func();
  const delta = performance.now() - start;
  info(`${logPrefix}: took ${delta} ms`);
  const stats = JSON.parse(await ChromeUtils.collectPerfStats());
  ChromeUtils.setPerfStatsCollectionMask(0);
  // Filter stuff out of stats that we don't care about.
  // Filter out the GPU process, since accessibility doesn't do anything there.
  stats.processes = stats.processes.filter(process => process.type != "gpu");
  for (const process of stats.processes) {
    // Because of weird JS -> WebIDL 64 bit number issues, we get metrics here
    // that aren't for accessibility. For example, 1 << 32 also gets us 1 << 0.
    // Filter those out. Also, filter out any metrics with a count of 0.
    process.perfstats.metrics = process.perfstats.metrics.filter(
      metric => metric.metric.startsWith("A11Y_") && metric.count > 0
    );
  }
  // Now that we've filtered metrics, remove any processes that have no metrics left.
  stats.processes = stats.processes.filter(
    process => !!process.perfstats.metrics.length
  );
  info(`${logPrefix}: PerfStats: ${JSON.stringify(stats, null, 2)}`);
}
