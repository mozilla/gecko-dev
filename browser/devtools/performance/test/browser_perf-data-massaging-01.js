/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the retrieved profiler data samples are correctly filtered and
 * normalized before passed to consumers.
 */

const WAIT_TIME = 1000; // ms

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let front = panel.panelWin.gFront;

  // Perform the first recording...

  let firstRecording = yield front.startRecording();
  let firstRecordingStartTime = firstRecording._profilerStartTime;
  info("Started profiling at: " + firstRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample some cpu activity

  yield front.stopRecording(firstRecording);

  ok(firstRecording.getDuration() >= WAIT_TIME,
    "The first recording duration is correct.");

  // Perform the second recording...

  let secondRecording = yield front.startRecording();
  let secondRecordingStartTime = secondRecording._profilerStartTime;
  info("Started profiling at: " + secondRecordingStartTime);

  busyWait(WAIT_TIME); // allow the profiler module to sample more cpu activity

  yield front.stopRecording(secondRecording);
  let secondRecordingProfile = secondRecording.getProfile();
  let secondRecordingSamples = secondRecordingProfile.threads[0].samples.data;

  isnot(secondRecording._profilerStartTime, 0,
    "The profiling start time should not be 0 on the second recording.");
  ok(secondRecording.getDuration() >= WAIT_TIME,
    "The second recording duration is correct.");

  const TIME_SLOT = secondRecordingProfile.threads[0].samples.schema.time;
  ok(secondRecordingSamples[0][TIME_SLOT] < secondRecordingStartTime,
    "The second recorded sample times were normalized.");
  ok(secondRecordingSamples[0][TIME_SLOT] > 0,
    "The second recorded sample times were normalized correctly.");
  ok(!secondRecordingSamples.find(e => e[TIME_SLOT] + secondRecordingStartTime <= firstRecording.getDuration()),
    "There should be no samples from the first recording in the second one, " +
    "even though the total number of frames did not overflow.");

  yield teardown(panel);
  finish();
}
