/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const selectors = require("devtools/client/performance-new/store/selectors");
const { recordingState: {
  AVAILABLE_TO_RECORD,
  REQUEST_TO_START_RECORDING,
  REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER,
  REQUEST_TO_STOP_PROFILER,
}, INFINITE_WINDOW_LENGTH } = require("devtools/client/performance-new/utils");

/**
 * The recording state manages the current state of the recording panel.
 * @param {string} state - A valid state in `recordingState`.
 * @param {object} options
 */
const changeRecordingState = exports.changeRecordingState =
  (state, options = { didRecordingUnexpectedlyStopped: false }) => ({
    type: "CHANGE_RECORDING_STATE",
    state,
    didRecordingUnexpectedlyStopped: options.didRecordingUnexpectedlyStopped,
  });

/**
 * This is the result of the initial questions about the state of the profiler.
 *
 * @param {boolean} isSupportedPlatform - This is a supported platform.
 * @param {string} recordingState - A valid state in `recordingState`.
 */
exports.reportProfilerReady = (isSupportedPlatform, recordingState) => ({
  type: "REPORT_PROFILER_READY",
  isSupportedPlatform,
  recordingState,
});

/**
 * Dispatch the given action, and then update the recording settings.
 * @param {object} action
 */
function _dispatchAndUpdatePreferences(action) {
  return (dispatch, getState) => {
    if (typeof action !== "object") {
      throw new Error(
        "This function assumes that the dispatched action is a simple object and " +
        "synchronous."
      );
    }
    dispatch(action);
    const setRecordingPreferences = selectors.getSetRecordingPreferencesFn(getState());
    const recordingSettings = selectors.getRecordingSettings(getState());
    setRecordingPreferences(recordingSettings);
  };
}

/**
 * Updates the recording settings for the interval.
 * @param {number} interval
 */
exports.changeInterval = interval => _dispatchAndUpdatePreferences({
  type: "CHANGE_INTERVAL",
  interval,
});

/**
 * Updates the recording settings for the entries.
 * @param {number} entries
 */
exports.changeEntries = entries => _dispatchAndUpdatePreferences({
  type: "CHANGE_ENTRIES",
  entries,
});

/**
 * Updates the recording settings for the duration.
 * @param {number} duration in seconds
 */
exports.changeDuration = duration => _dispatchAndUpdatePreferences({
  type: "CHANGE_DURATION",
  duration,
});

/**
 * Updates the recording settings for the features.
 * @param {object} features
 */
exports.changeFeatures = features => _dispatchAndUpdatePreferences({
  type: "CHANGE_FEATURES",
  features,
});

/**
 * Updates the recording settings for the threads.
 * @param {array} threads
 */
exports.changeThreads = threads => _dispatchAndUpdatePreferences({
  type: "CHANGE_THREADS",
  threads,
});

/**
 * Receive the values to intialize the store. See the reducer for what values
 * are expected.
 * @param {object} threads
 */
exports.initializeStore = values => ({
  type: "INITIALIZE_STORE",
  ...values,
});

/**
 * Start a new recording with the perfFront and update the internal recording state.
 */
exports.startRecording = () => {
  return (dispatch, getState) => {
    const recordingSettings = selectors.getRecordingSettings(getState());
    const perfFront = selectors.getPerfFront(getState());
    // We should pass 0 to startProfiler call if the window length should be infinite.
    if (recordingSettings.duration === INFINITE_WINDOW_LENGTH) {
      recordingSettings.duration = 0;
    }
    // Firefox 65 introduced a duration-based buffer with actorVersion 1.
    // We should delete the duration parameter if the profiled Firefox is older than
    // version 1. This cannot happen inside the devtools panel but it may happen
    // when profiling an older Firefox with remote debugging. Fx65+
    if (selectors.getActorVersion(getState()) < 1) {
      delete recordingSettings.duration;
    }
    perfFront.startProfiler(recordingSettings);
    dispatch(changeRecordingState(REQUEST_TO_START_RECORDING));
  };
};

/**
 * Returns a function getDebugPathFor(debugName, breakpadId) => string which
 * resolves a (debugName, breakpadId) pair to the library's debugPath, i.e.
 * the path on the file system where the binary is stored.
 *
 * This is needed for the following reason:
 *  - In order to obtain a symbol table for a system library, we need to know
 *    the library's absolute path on the file system.
 *  - Symbol tables are requested asynchronously, by the profiler UI, after the
 *    profile itself has been obtained.
 *  - When the symbol tables are requested, we don't want the profiler UI to
 *    pass us arbitrary absolute file paths, as an extra defense against
 *    potential information leaks.
 *  - Instead, when the UI requests symbol tables, it identifies the library
 *    with a (debugName, breakpadId) pair. We need to map that pair back to the
 *    absolute path of that library.
 *  - We get the "trusted" paths from the "libs" sections of the profile. We
 *    trust these paths because we just obtained the profile directly from
 *    Gecko.
 *  - This function builds the (debugName, breakpadId) => debugPath mapping and
 *    retains it on the returned closure so that it can be consulted after the
 *    profile has been passed to the UI.
 *
 * @param {object} profile - The profile JSON object
 */
function createDebugPathMapForLibsInProfile(profile) {
  const map = new Map();
  function fillMapForProcessRecursive(processProfile) {
    for (const lib of processProfile.libs) {
      const { debugName, debugPath, breakpadId } = lib;
      const key = [debugName, breakpadId].join(":");
      map.set(key, debugPath);
    }
    for (const subprocess of processProfile.processes) {
      fillMapForProcessRecursive(subprocess);
    }
  }

  fillMapForProcessRecursive(profile);
  return function getDebugPathFor(debugName, breakpadId) {
    const key = [debugName, breakpadId].join(":");
    return map.get(key);
  };
}

/**
 * Stops the profiler, and opens the profile in a new window.
 */
exports.getProfileAndStopProfiler = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState(REQUEST_TO_GET_PROFILE_AND_STOP_PROFILER));
    const profile = await perfFront.getProfileAndStopProfiler();

    const debugPathGetter = createDebugPathMapForLibsInProfile(profile);
    async function getSymbolTable(debugName, breakpadId) {
      const debugPath = debugPathGetter(debugName, breakpadId);
      const [addresses, index, buffer] =
        await perfFront.getSymbolTable(debugPath, breakpadId);
      // The protocol transmits these arrays as plain JavaScript arrays of
      // numbers, but we want to pass them on as typed arrays. Convert them now.
      return [
        new Uint32Array(addresses),
        new Uint32Array(index),
        new Uint8Array(buffer),
      ];
    }

    selectors.getReceiveProfileFn(getState())(profile, getSymbolTable);
    dispatch(changeRecordingState(AVAILABLE_TO_RECORD));
  };
};

/**
 * Stops the profiler, but does not try to retrieve the profile.
 */
exports.stopProfilerAndDiscardProfile = () => {
  return async (dispatch, getState) => {
    const perfFront = selectors.getPerfFront(getState());
    dispatch(changeRecordingState(REQUEST_TO_STOP_PROFILER));
    perfFront.stopProfilerAndDiscardProfile();
  };
};
