/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let setupSignal;
add_setup(function () {
  ok(testSignal instanceof AbortSignal, "Should get an AbortSignal");
  ok(!testSignal.aborted, "signal should not be aborted");
  setupSignal = testSignal;

  registerCleanupFunction(() => {
    ok(setupSignal.aborted, "The setup abort signal should be aborted");
  });
});

let prevSignal;
add_task(function () {
  ok(!setupSignal.aborted, "The setup abort signal should not be aborted");
  ok(testSignal instanceof AbortSignal, "Should get an AbortSignal");
  isnot(testSignal, setupSignal, "Should get a new signal");
  ok(!testSignal.aborted, "signal should not be aborted");
  prevSignal = testSignal;
});

add_task(function () {
  ok(prevSignal.aborted, "The previous signal should not be aborted");
  ok(testSignal instanceof AbortSignal, "Should get an AbortSignal");
  isnot(testSignal, prevSignal, "Should get a new signal");
  ok(!testSignal.aborted, "signal should not be aborted");
});
