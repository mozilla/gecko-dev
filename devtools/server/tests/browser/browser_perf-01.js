/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test is at the edge of timing out, probably because of LUL
// initialization on Linux. This is also happening only once, which is why only
// this test needs it: for other tests LUL is already initialized because
// they're running in the same Firefox instance.
// See also bug 1635442.
requestLongerTimeout(2);

async function decompressGzip(buffer) {
  if (buffer.resizable) {
    // Not sure why, but we can't use a resizable buffer for streams.
    buffer = buffer.transferToFixedLength();
  }
  const decompressionStream = new DecompressionStream("gzip");
  const decoderStream = new TextDecoderStream();
  const decodedStream = decompressionStream.readable.pipeThrough(decoderStream);
  const writer = decompressionStream.writable.getWriter();
  writer.write(buffer);
  const writePromise = writer.close();

  let result = "";
  for await (const chunk of decodedStream) {
    result += chunk;
  }

  await writePromise;
  return JSON.parse(result);
}

/**
 * Run through a series of basic recording actions for the perf actor.
 */
add_task(async function () {
  const { front, client } = await initPerfFront();

  // Assert the initial state.
  is(
    await front.isSupportedPlatform(),
    true,
    "This test only runs on supported platforms."
  );
  is(await front.isActive(), false, "The profiler is not active yet.");

  // Start the profiler.
  const profilerStarted = once(front, "profiler-started");
  await front.startProfiler();
  await profilerStarted;
  is(await front.isActive(), true, "The profiler was started.");

  // Stop the profiler and assert the results.
  const profilerStopped1 = once(front, "profiler-stopped");
  const { profile: gzippedProfile, additionalInformation } =
    await front.getProfileAndStopProfiler();
  await profilerStopped1;
  is(await front.isActive(), false, "The profiler was stopped.");
  const profile = await decompressGzip(gzippedProfile);
  ok("threads" in profile, "The actor was used to record a profile.");
  ok(
    additionalInformation.sharedLibraries,
    "We retrieved some shared libraries as well."
  );

  // Restart the profiler.
  await front.startProfiler();
  is(await front.isActive(), true, "The profiler was re-started.");

  // Stop and discard.
  const profilerStopped2 = once(front, "profiler-stopped");
  await front.stopProfilerAndDiscardProfile();
  await profilerStopped2;
  is(
    await front.isActive(),
    false,
    "The profiler was stopped and the profile discarded."
  );

  await front.destroy();
  await client.close();
});

add_task(async function test_error_case() {
  const { front, client } = await initPerfFront();

  try {
    // We try to get the profile without starting the profiler first. This should
    // trigger an error in the our C++ code.
    await front.getProfileAndStopProfiler();
    ok(false, "Getting the profile should fail");
  } catch (e) {
    Assert.stringContains(
      e.message,
      "The profiler is not active.",
      "The error contains the expected error message."
    );
  }

  await front.destroy();
  await client.close();
});
