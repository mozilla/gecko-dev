/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getAllTraces,
  getTraceFrames,
  getIsCurrentlyTracing,
  getCurrentThread,
} from "../selectors/index";
import { NO_SEARCH_VALUE } from "../reducers/tracer-frames";

import { selectSourceBySourceActorID } from "./sources/select.js";
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

/**
 * Called when tracing is toggled ON/OFF on a particular thread.
 */
export function tracingToggled(thread, enabled, traceValues) {
  return {
    type: "TRACING_TOGGLED",
    thread,
    enabled,
    traceValues,
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
    // For now, the tracer only consider the top level thread
    const thread = getCurrentThread(getState());

    dispatch({
      type: "SELECT_TRACE",
      traceIndex,
      thread,
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

export function searchTraceArguments(searchString) {
  return async function ({ dispatch, client, panel }) {
    // Ignore any starting and ending spaces in the query string
    searchString = searchString.trim();

    // Reset back to no search if the query is empty
    if (!searchString) {
      dispatch({
        type: "SET_TRACE_SEARCH_STRING",
        searchValueOrGrip: NO_SEARCH_VALUE,
      });
      return;
    }

    // `JSON.parse("undefined")` throws, but we still want to support searching for this special value
    // without having to evaluate to the server
    if (searchString === "undefined") {
      dispatch({
        type: "SET_TRACE_SEARCH_STRING",
        searchValueOrGrip: undefined,
      });
      return;
    }

    // First check on the frontend if that's a primitive,
    // in which case, we can compute the value without evaling in the server.
    try {
      const value = JSON.parse(searchString);
      // Ignore any object value, as we won't have any match anyway.
      // We can only search for existing page objects.
      if (typeof value == "object" && value !== null) {
        dispatch({
          type: "SET_TRACE_SEARCH_EXCEPTION",
          errorMessage:
            "Invalid search. Can only search for existing page JS objects",
        });
        return;
      }
      dispatch({
        type: "SET_TRACE_SEARCH_STRING",
        searchValueOrGrip: value,
      });
      return;
    } catch (e) {}

    // If the inspector is opened, and a node is currently selected,
    // try to fetch its actor ID in order to make '$0' to work in evaluations
    const inspector = panel.toolbox.getPanel("inspector");
    const selectedNodeActor = inspector?.selection?.nodeFront?.actorID;

    let { result, exception } = await client.evaluate(`(${searchString})`, {
      selectedNodeActor,
      evalInTracer: true,
    });

    if (result.type == "null") {
      result = null;
    } else if (result.type == "undefined") {
      result = undefined;
    }

    if (exception) {
      const { preview } = exception.getGrip();
      const errorMessage = `${preview.name}: ${preview.message}`;
      dispatch({
        type: "SET_TRACE_SEARCH_EXCEPTION",
        errorMessage,
      });
    } else {
      // If we refered to an object, the `result` will be an ObjectActorFront
      // for which we retrieve its current "form" (a.k.a. grip).
      // Otherwise `result` will be a primitive JS value (boolean, number, string,...)
      const searchValueOrGrip =
        result && result.getGrip ? result.getGrip() : result;

      dispatch({
        type: "SET_TRACE_SEARCH_STRING",
        searchValueOrGrip,
      });
    }
  };
}
