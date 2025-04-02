/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("../../client/performance-new/@types/perf").GetActiveBrowserID} GetActiveBrowserID
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

// This file also exports a named object containing other exports to play well
// with defineESModuleGetters.
export const RecordingUtils = {
  getActiveBrowserID,
};
