/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

function initialState() {
  return {
    // These fields are mutable as they are large arrays and UI will rerender based on their size

    // The three next array are always of the same size.
    // List of all trace resources, as defined by the server codebase (See the TracerActor)
    mutableTraces: [],
    // Array of arrays. This is of the same size as mutableTraces.
    // Store the indexes within mutableTraces of each children matching the same index in mutableTraces.
    mutableChildren: [],
    // Indexes of parents within mutableTraces.
    mutableParents: [],

    // Frames are also a trace resources, but they are stored in a dedicated array.
    mutableFrames: [],

    // List of indexes within mutableTraces of top level trace, without any parent.
    mutableTopTraces: [],

    // List of all trace resources indexes within mutableTraces which are about dom mutations
    mutableMutationTraces: [],

    // Index of the currently selected trace within `mutableTraces`.
    selectedTraceIndex: null,
  };
}

function update(state = initialState(), action) {
  switch (action.type) {
    case "TRACING_TOGGLED": {
      if (action.enabled) {
        return initialState();
      }
      return state;
    }

    case "TRACING_CLEAR": {
      return initialState();
    }

    case "ADD_TRACES": {
      addTraces(state, action.traces);
      return { ...state };
    }

    case "SELECT_TRACE": {
      if (
        action.traceIndex < 0 ||
        action.traceIndex >= state.mutableTraces.length
      ) {
        return state;
      }
      return {
        ...state,
        selectedTraceIndex: action.traceIndex,
      };
    }

    case "SET_SELECTED_LOCATION": {
      // Traces are reference to the generated location only, so ignore any original source being selected
      // and wait for SET_GENERATED_SELECTED_LOCATION instead.
      if (action.location.source.isOriginal) return state;

      // Ignore if the currently selected trace matches the new location.
      if (
        state.selectedTrace &&
        locationMatchTrace(action.location, state.selectedTrace)
      ) {
        return state;
      }

      // Lookup for a trace matching the newly selected location
      for (const trace of state.mutableTraces) {
        if (locationMatchTrace(action.location, trace)) {
          return {
            ...state,
            selectedTrace: trace,
          };
        }
      }

      return {
        ...state,
        selectedTrace: null,
      };
    }

    case "SET_GENERATED_SELECTED_LOCATION": {
      // When selecting an original location, we have to wait for the newly selected original location
      // to be mapped to a generated location so that we can find a matching trace.

      // Ignore if the currently selected trace matches the new location.
      if (
        state.selectedTrace &&
        locationMatchTrace(action.generatedLocation, state.selectedTrace)
      ) {
        return state;
      }

      // Lookup for a trace matching the newly selected location
      for (const trace of state.mutableTraces) {
        if (locationMatchTrace(action.generatedLocation, trace)) {
          return {
            ...state,
            selectedTrace: trace,
          };
        }
      }

      return {
        ...state,
        selectedTrace: null,
      };
    }

    case "CLEAR_SELECTED_LOCATION": {
      return {
        ...state,
        selectedTrace: null,
      };
    }
  }
  return state;
}

function addTraces(state, traces) {
  const {
    mutableTraces,
    mutableMutationTraces,
    mutableFrames,
    mutableTopTraces,
    mutableChildren,
    mutableParents,
  } = state;

  function matchParent(traceIndex, depth) {
    // The very last element is the one matching traceIndex,
    // so pick the one added just before.
    // We consider that traces are reported by the server in the execution order.
    let idx = mutableTraces.length - 2;
    while (idx != null) {
      const trace = mutableTraces[idx];
      if (!trace) {
        break;
      }
      const currentDepth = trace[TRACER_FIELDS_INDEXES.DEPTH];
      if (currentDepth < depth) {
        mutableChildren[idx].push(traceIndex);
        mutableParents.push(idx);
        return;
      }
      idx = mutableParents[idx];
    }

    // If no parent was found, flag it as top level trace
    mutableTopTraces.push(traceIndex);
    mutableParents.push(null);
  }
  for (const traceResource of traces) {
    // For now, only consider traces from the top level target/thread
    if (!traceResource.targetFront.isTopLevel) {
      continue;
    }

    const type = traceResource[TRACER_FIELDS_INDEXES.TYPE];

    switch (type) {
      case "frame": {
        // Store the object used by SmartTraces
        mutableFrames.push({
          functionDisplayName: traceResource[TRACER_FIELDS_INDEXES.FRAME_NAME],
          source: traceResource[TRACER_FIELDS_INDEXES.FRAME_URL],
          sourceId: traceResource[TRACER_FIELDS_INDEXES.FRAME_SOURCEID],
          line: traceResource[TRACER_FIELDS_INDEXES.FRAME_LINE],
          column: traceResource[TRACER_FIELDS_INDEXES.FRAME_COLUMN],
        });
        break;
      }

      case "enter": {
        const traceIndex = mutableTraces.length;
        mutableTraces.push(traceResource);
        mutableChildren.push([]);
        const depth = traceResource[TRACER_FIELDS_INDEXES.DEPTH];
        matchParent(traceIndex, depth);
        break;
      }

      case "exit": {
        // The sidebar doesn't use this information yet
        break;
      }

      case "dom-mutation": {
        const traceIndex = mutableTraces.length;
        mutableTraces.push(traceResource);
        mutableChildren.push([]);
        mutableMutationTraces.push(traceIndex);

        const depth = traceResource[TRACER_FIELDS_INDEXES.DEPTH];
        matchParent(traceIndex, depth);
        break;
      }

      case "event": {
        const traceIndex = mutableTraces.length;
        mutableTraces.push(traceResource);
        mutableChildren.push([]);
        mutableParents.push(null);
        mutableTopTraces.push(traceIndex);
        break;
      }
    }
  }
}

function locationMatchTrace(location, trace) {
  return (
    trace.sourceId == location.sourceActor.id &&
    trace.lineNumber == location.line &&
    trace.columnNumber == location.column
  );
}

export default update;
