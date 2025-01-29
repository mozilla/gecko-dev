/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_REQUEST,
  ADD_TIMING_MARKER,
  CLEAR_TIMING_MARKERS,
  CLEAR_REQUESTS,
} = require("resource://devtools/client/netmonitor/src/constants.js");

function TimingMarkers() {
  return {
    firstDocumentDOMContentLoadedTimestamp: -1,
    firstDocumentLoadTimestamp: -1,
    firstDocumentRequestStartTimestamp: +Infinity,
  };
}

function addRequest(state, action) {
  const { startedMs } = action.data;
  if (startedMs < state.firstDocumentRequestStartTimestamp) {
    return {
      ...state,
      firstDocumentRequestStartTimestamp: startedMs,
    };
  }

  return state;
}

function addTimingMarker(state, action) {
  if (
    action.marker.name === "dom-interactive" &&
    state.firstDocumentDOMContentLoadedTimestamp === -1
  ) {
    return {
      ...state,
      firstDocumentDOMContentLoadedTimestamp: action.marker.time,
    };
  }

  if (
    action.marker.name === "dom-complete" &&
    state.firstDocumentLoadTimestamp === -1
  ) {
    return {
      ...state,
      firstDocumentLoadTimestamp: action.marker.time,
    };
  }

  return state;
}

function clearTimingMarkers() {
  return new TimingMarkers();
}

function timingMarkers(state = new TimingMarkers(), action) {
  switch (action.type) {
    case ADD_REQUEST:
      return addRequest(state, action);

    case ADD_TIMING_MARKER:
      return addTimingMarker(state, action);

    case CLEAR_REQUESTS:
    case CLEAR_TIMING_MARKERS:
      return clearTimingMarkers(state);

    default:
      return state;
  }
}

module.exports = {
  TimingMarkers,
  timingMarkers,
};
