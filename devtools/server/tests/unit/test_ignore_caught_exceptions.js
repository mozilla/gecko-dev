/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

/**
 * Test that setting ignoreCaughtExceptions will cause the debugger to ignore
 * caught exceptions, but not uncaught ones.
 */

add_task(threadClientTest(async ({ threadClient, client, debuggee }) => {
  await executeOnNextTickAndWaitForPause(() => evaluateTestCode(debuggee), client);

  threadClient.pauseOnExceptions(true, true);
  await resume(threadClient);
  const paused = await waitForPause(client);
  Assert.equal(paused.why.type, "exception");
  equal(paused.frame.where.line, 6, "paused at throw");

  await resume(threadClient);
}, {
  // Bug 1508289, exception tests fails in worker scope
  doNotRunWorker: true,
}));

function evaluateTestCode(debuggee) {
  /* eslint-disable */
  try {
  Cu.evalInSandbox(`                    // 1
   debugger;                            // 2
   try {                                // 3           
     throw "foo";                       // 4
   } catch (e) {}                       // 5
   throw "bar";                         // 6  
  `,                                    // 7
    debuggee,
    "1.8",
    "test_pause_exceptions-03.js",
    1
  );
  } catch (e) {}
  /* eslint-disable */
}
