/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("perf").GetActiveBrowserID} GetActiveBrowserID
 * @typedef {import("perf").ProfileCaptureResult} ProfileCaptureResult
 */

/**
 * Gets the ID of active tab from the browser.
 *
 * @type {GetActiveBrowserID}
 */
export function getActiveBrowserID() {
  const win = Services.wm.getMostRecentWindow("navigator:browser");

  const browserId = win?.gBrowser?.selectedBrowser?.browsingContext?.browserId;
  if (browserId) {
    return browserId;
  }

  console.error(
    "Failed to get the active browserId while starting the profiler."
  );
  // `0` mean that we failed to ge the active browserId, and it's
  // treated as null value in the platform.
  return 0;
}

/**
 * @typedef {Object} ProfileCaptureResultAndAdditionalInformation
 * @property {ProfileCaptureResult} profileCaptureResult
 * @property {MockedExports.ProfileGenerationAdditionalInformation} [additionalInformation]
 */
/**
 * Fetch the profile data from Firefox, then stop the profiler.
 *
 * @returns {Promise<ProfileCaptureResultAndAdditionalInformation>}
 */
export async function getProfileDataAsGzippedArrayBufferThenStop() {
  if (!Services.profiler.IsActive()) {
    // The profiler is not active, ignore.
    return {
      profileCaptureResult: {
        type: "ERROR",
        error: new Error("The profiler is not active."),
      },
    };
  }
  if (Services.profiler.IsPaused()) {
    // The profiler is already paused for capture, ignore.
    return {
      profileCaptureResult: {
        type: "ERROR",
        error: new Error("The profiler is already paused."),
      },
    };
  }

  // Pause profiler before we collect the profile, so that we don't capture
  // more samples while the parent process waits for subprocess profiles.
  Services.profiler.Pause();

  try {
    const { profile, additionalInformation } =
      await Services.profiler.getProfileDataAsGzippedArrayBuffer();

    return {
      profileCaptureResult: { type: "SUCCESS", profile },
      additionalInformation,
    };
  } catch (unknownError) {
    const error = /** @type {Error} */ (unknownError);
    return { profileCaptureResult: { type: "ERROR", error } };
  } finally {
    // We're purposefully not using `await`, to minimize the time the user has
    // to wait until the result is returned.
    Services.profiler.StopProfiler();
  }
}

// This file also exports a named object containing other exports to play well
// with defineESModuleGetters.
export const RecordingUtils = {
  getActiveBrowserID,
  getProfileDataAsGzippedArrayBufferThenStop,
};
