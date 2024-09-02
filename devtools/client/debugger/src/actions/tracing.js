/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getAllTraces,
  getTraceFrames,
  getIsCurrentlyTracing,
} from "../selectors/index";
import { selectSourceBySourceActorID } from "./sources/select.js";
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

/**
 * Called when tracing is toggled ON/OFF on a particular thread.
 */
export function tracingToggled(thread, enabled) {
  return {
    type: "TRACING_TOGGLED",
    thread,
    enabled,
  };
}

export function clearTracerData() {
  return {
    type: "TRACING_CLEAR",
  };
}

export function addTraces(traces) {
  return async function ({ dispatch, getState }) {
    if (!getIsCurrentlyTracing(getState())) {
      return null;
    }

    return dispatch({
      type: "ADD_TRACES",
      traces,
    });
  };
}

export function selectTrace(traceIndex) {
  return async function ({ dispatch, getState }) {
    dispatch({
      type: "SELECT_TRACE",
      traceIndex,
    });
    const traces = getAllTraces(getState());
    const trace = traces[traceIndex];
    // Ignore DOM Event traces, which aren't related to a particular location in source.
    if (!trace || trace[TRACER_FIELDS_INDEXES.TYPE] == "event") {
      return;
    }

    const frameIndex = trace[TRACER_FIELDS_INDEXES.FRAME_INDEX];
    const frames = getTraceFrames(getState());
    const frame = frames[frameIndex];

    await dispatch(
      selectSourceBySourceActorID(frame.sourceId, {
        line: frame.line,
        column: frame.column,
      })
    );
  };
}

export function setLocalAndRemoteRuntimeVersion(
  localPlatformVersion,
  remotePlatformVersion
) {
  return {
    type: "SET_RUNTIME_VERSIONS",
    localPlatformVersion,
    remotePlatformVersion,
  };
}
