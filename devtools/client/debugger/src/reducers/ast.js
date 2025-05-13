/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Ast reducer
 * @module reducers/ast
 */

import { makeBreakpointId } from "../utils/breakpoint/index";

export function initialASTState() {
  return {
    // We are using mutable objects as we never return the dictionary as-is from the selectors
    // but only their values.
    // Note that all these dictionaries are storing objects as values
    // which all will have a threadActorId attribute.
    mutableInScopeLines: {},
  };
}

function update(state = initialASTState(), action) {
  switch (action.type) {
    case "IN_SCOPE_LINES": {
      state.mutableInScopeLines[makeBreakpointId(action.location)] = {
        lines: action.lines,
        threadActorId: action.location.sourceActor?.thread,
      };
      return {
        ...state,
      };
    }

    case "RESUME": {
      return { ...state, mutableInScopeLines: {} };
    }

    case "REMOVE_THREAD": {
      function clearDict(dict, threadId) {
        for (const key in dict) {
          if (dict[key].threadActorId == threadId) {
            delete dict[key];
          }
        }
      }

      clearDict(state.mutableInScopeLines, action.threadActorID);
      return { ...state };
    }

    default: {
      return state;
    }
  }
}

export default update;
