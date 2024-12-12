/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function check_profile_contains_parent_and_content_pids(
  parent_pid,
  content_pid,
  profile
) {
  info(
    `Checking that the profile contains pids for the parent process ${parent_pid} and content process ${content_pid}`
  );
  Assert.equal(
    profile.threads[0].pid,
    parent_pid,
    "We expect the pid of the main profile thread to be the parent pid"
  );

  // Keep a record of the pids found in the profile, so that we can give a
  // better error message.
  const child_pids = [];
  let found = false;
  for (const process of profile.processes) {
    child_pids.push(process.threads[0].pid);
    if (process.threads[0].pid === content_pid) {
      found = true;
      info(`Found content pid: ${process.threads[0].pid}.`);
    }
  }

  Assert.ok(
    found,
    `We expect the child process ids to contain the id of the content process (${content_pid}). Actually found: ${child_pids}`
  );
}

function check_profile_for_synthetic_marker(profile) {
  // Essentially the same test as `browser_test_markers_parent_process.js`
  const markers = ProfilerTestUtils.getInflatedMarkerData(profile.threads[0]);
  {
    const domEventStart = markers.find(
      ({ phase, data }) =>
        phase === ProfilerTestUtils.markerPhases.INTERVAL_START &&
        data?.eventType === "synthetic"
    );
    const domEventEnd = markers.find(
      ({ phase, data }) =>
        phase === ProfilerTestUtils.markerPhases.INTERVAL_END &&
        data?.eventType === "synthetic"
    );
    ok(domEventStart, "A start DOMEvent was generated");
    ok(domEventEnd, "An end DOMEvent was generated");
    Assert.greater(
      domEventEnd.data.latency,
      0,
      "DOMEvent had a a latency value generated."
    );
    Assert.strictEqual(domEventEnd.data.type, "DOMEvent");
    Assert.strictEqual(domEventEnd.name, "DOMEvent");
  }
}

// Test signal handling within the profiler: Start the profiler with a POSIX
// signal, and capture a profile with a POSIX signal. Along the way, record a
// marker from a synthetic dom event. Check that the marker shows up in the
// final profile, and that the processes that we expect to see are also there.
//
// We would ideally like to have three tests for the three following scenarios:
//
// 1) Starting the profiler normally, and stopping it with a signal,
// 2) Starting the profiler with a signal, and stopping it normally
// 3) Both starting and stopping the profiler with a signal
//
// That way, if any one of the three tests fails, we can quickly isolate which
// part of the signal-handling code has failed. If (1 & 3) fail, then it's the
// stopping code, if (2 & 3) fail, it's the starting code, and if just (3)
// fails, it's something else entirely. However, this would use up a lot of time
// in CI, so instead we just have test (3). This can be easily modified to act
// like (1) or (2) when debugging.
//
// - To make this test act like (1), replace the the call to
//   `raiseSignal(ppid.pid, SIGUSR1)` with (e.g.) `await startProfiler({
//   features: [""], threads: ["GeckoMain"] });`
// - To make this test act like (2), replace the call to `raiseSignal(ppid.pid,
//   SIGUSR2)` etc with (e.g.) `const profile = await stopNowAndGetProfile();`
//

add_task(
  async function test_profile_feature_multiprocess_start_and_capture_with_signal() {
    Assert.ok(
      !Services.profiler.IsActive(),
      "The profiler is not currently active"
    );

    let ppid = await ChromeUtils.requestProcInfo();
    let parent_pid = ppid.pid;

    let startPromise = TestUtils.topicObserved("profiler-started");

    info(`Raising signal SIGUSR1 with pid ${parent_pid} to start the profiler`);
    // Try and start the profiler using a signal.
    let result = raiseSignal(parent_pid, SIGUSR1);
    Assert.ok(result, "Raising a signal should succeed");

    // Wait for the profiler to stop
    Assert.ok(await startPromise, "The profiler should start");

    // Wait until the profiler is active
    Assert.ok(
      Services.profiler.IsActive(),
      "The profiler should now be active."
    );

    const url = BASE_URL + "do_work_500ms.html";
    await BrowserTestUtils.withNewTab(url, async contentBrowser => {
      info("Finding the PId of the content process.");
      const content_pid = await SpecialPowers.spawn(
        contentBrowser,
        [],
        () => Services.appinfo.processID
      );

      // Dispatch a synthetic event so that we can search for the marker in the
      // profile
      info("Dispatching a synthetic DOMEvent");
      window.dispatchEvent(new Event("synthetic"));

      // Wait 500ms so that the tab finishes executing.
      info("Waiting for the tab to do some work");
      await wait(500);

      // Set up an observer to watch for the profiler stopping
      let stopPromise = TestUtils.topicObserved("profiler-stopped");

      // Try and stop the profiler using a signal.
      info(
        `Raising signal SIGUSR2 with pid ${parent_pid} to stop the profiler`
      );
      let result = raiseSignal(parent_pid, SIGUSR2);
      Assert.ok(result, "Raising a SIGUSR2 signal should succeed.");

      // Wait for the profiler to stop
      info(`Waiting for the profiler to stop.`);
      Assert.ok(await stopPromise, "The profiler should stop");

      // Check that we have a profile written to disk:
      info(`Retrieving profile file.`);
      let profile_file = await getFullProfilePath(parent_pid);
      Assert.ok(
        await IOUtils.exists(profile_file),
        "A profile file should be written to disk."
      );

      // Read the profile from the json file
      let profile = await IOUtils.readJSON(profile_file);
      info("Found this many proceses: " + profile.processes.length);

      // check for processes and the synthetic marker
      info(
        `Checking that the profile file contains the parent and content processes.`
      );
      check_profile_contains_parent_and_content_pids(
        parent_pid,
        content_pid,
        profile
      );

      info(`Checking for the synthetic DOM marker`);
      check_profile_for_synthetic_marker(profile);
    });
  }
);
