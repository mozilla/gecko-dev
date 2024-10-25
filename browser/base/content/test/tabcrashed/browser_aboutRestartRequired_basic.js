"use strict";

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(5);

SimpleTest.expectChildProcessCrash();

async function execTest() {
  is(
    await getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );
  is(
    await getTrueMismatchTelemetry(),
    undefined,
    "Build ID true mismatch count should be undefined"
  );

  await forceCleanProcesses();
  let eventPromise = getEventPromise("oop-browser-crashed", "basic");
  let tab = await openNewTab(true);
  await eventPromise;

  is(
    await getTrueMismatchTelemetry(),
    undefined,
    "Build ID true mismatch count should be undefined"
  );
  is(
    await getFalsePositiveTelemetry(),
    undefined,
    "Build ID mismatch false positive count should be undefined"
  );

  await closeTab(tab);
}

add_task(async function test_telemetry_restartrequired_no_mismatch() {
  // Do not clear telemetry's scalars, otherwise --verify will break because
  // the parent process will have kept memory of sent telemetry but that test
  // will not be aware

  info("Waiting for oop-browser-crashed event.");

  // Run once
  await execTest();
  // Run a second time and make sure it has not increased
  await execTest();
});
