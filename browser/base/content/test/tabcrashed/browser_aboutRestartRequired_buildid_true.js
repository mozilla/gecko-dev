"use strict";

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(5);

SimpleTest.expectChildProcessCrash();

async function execTest(expectedValueAfter) {
  setBuildidMismatchEnv();
  setBuildidMatchDontSendEnv();
  await forceCleanProcesses();
  let eventPromise = getEventPromise(
    "oop-browser-buildid-mismatch",
    "real-mismatch"
  );
  let tab = await openNewTab(false);
  await eventPromise;
  unsetBuildidMatchDontSendEnv();
  unsetBuildidMismatchEnv();

  is(
    await getTrueMismatchTelemetry(),
    expectedValueAfter,
    `Build ID true mismatch count should be ${expectedValueAfter}`
  );

  await closeTab(tab);
}

add_task(async function test_telemetry_restartrequired_real_mismatch() {
  // Do not clear telemetry's scalars, otherwise --verify will break because
  // the parent process will have kept memory of sent telemetry but that test
  // will not be aware

  info("Waiting for oop-browser-buildid-mismatch event.");

  // Run once
  await execTest(1);
  // Run a second time and make sure it has not increased
  await execTest(1);
});
