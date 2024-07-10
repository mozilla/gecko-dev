/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

async function cleanupAfterTest() {
  // We need to cleanup written profiles after a test
  // Get the system downloads directory, and use it to build a profile file
  let profile = FileUtils.File(await Downloads.getSystemDownloadsDirectory());

  // Get the process ID
  let pid = Services.appinfo.processID;

  // write it to the profile file name
  profile.append(`profile_0_${pid}.json`);

  // remove the file!
  await IOUtils.remove(profile.path, { ignoreAbsent: true });

  // Make sure the profiler is fully stopped, even if the test failed
  await Services.profiler.StopProfiler();
}

add_task(async () => {
  info("Test that starting the profiler with a posix signal works.");

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should not begin the test active."
  );

  // Get the process ID
  let pid = Services.appinfo.processID;

  // Set up an observer to watch for the profiler starting
  let startPromise = TestUtils.topicObserved("profiler-started");

  // Try and start the profiler using a signal.
  let result = raiseSignal(pid, SIGUSR1);
  Assert.ok(result, "Raising a signal should succeed");

  // Wait for the profiler to stop
  Assert.ok(await startPromise, "The profiler should start");

  // Wait until the profiler is active
  Assert.ok(Services.profiler.IsActive(), "The profiler should now be active.");

  // Let the profiler sample at least once
  await Services.profiler.waitOnePeriodicSampling();
  info("Waiting a periodic sampling completed");

  // Stop the profiler
  await Services.profiler.StopProfiler();
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should now be inactive."
  );
});

add_task(async () => {
  info("Test that stopping the profiler with a posix signal works.");
  registerCleanupFunction(cleanupAfterTest);

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should not begin the test active."
  );

  const entries = 100;
  const interval = 1;
  const threads = [];
  const features = [];

  // Start the profiler, and ensure that it's active
  await Services.profiler.StartProfiler(entries, interval, threads, features);
  Assert.ok(Services.profiler.IsActive(), "The profiler should now be active.");

  // Get the process ID
  let pid = Services.appinfo.processID;

  // Set up an observer to watch for the profiler stopping
  let stopPromise = TestUtils.topicObserved("profiler-stopped");

  // Try and stop the profiler using a signal.
  let result = raiseSignal(pid, SIGUSR2);
  Assert.ok(result, "Raising a SIGUSR2 signal should succeed.");

  // Wait for the profiler to stop
  Assert.ok(await stopPromise, "The profiler should stop");

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should now be inactive."
  );
});

add_task(async () => {
  info(
    "Test that stopping the profiler with a posix signal writes a profile file to the system download directory."
  );
  registerCleanupFunction(cleanupAfterTest);

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should not begin the test active."
  );

  const entries = 100;
  const interval = 1;
  const threads = [];
  const features = [];

  // Get the system downloads directory, and use it to build a profile file
  let profile = FileUtils.File(await Downloads.getSystemDownloadsDirectory());

  // Get the process ID
  let pid = Services.appinfo.processID;

  // use the pid to construct the name of the profile, and resulting file
  profile.append(`profile_0_${pid}.json`);

  // Start the profiler, and ensure that it's active
  await Services.profiler.StartProfiler(entries, interval, threads, features);
  Assert.ok(Services.profiler.IsActive(), "The profiler should now be active.");

  // Set up an observer to watch for the profiler stopping
  let stopPromise = TestUtils.topicObserved("profiler-stopped");

  // Try and stop the profiler using a signal.
  let result = raiseSignal(pid, SIGUSR2);
  Assert.ok(result, "Raising a SIGUSR2 signal should succeed.");

  // Wait for the profiler to stop
  Assert.ok(await stopPromise, "The profiler should stop");

  // Now that it's stopped, make sure that we have a profile file
  Assert.ok(
    await IOUtils.exists(profile.path),
    "A profile file should be written to disk."
  );

  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler should now be inactive."
  );
});
