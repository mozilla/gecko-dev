/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  DEFAULT_FILTERS_RESET,
} = require("devtools/client/webconsole/constants");

/**
 * Event telemetry middleware is responsible for logging specific events to telemetry.
 */
function eventTelemetryMiddleware(telemetry, sessionId, store) {
  return next => action => {
    const oldState = store.getState();
    const res = next(action);
    if (!sessionId) {
      return res;
    }

    const state = store.getState();

    const filterChangeActions = [
      FILTER_TEXT_SET,
      FILTER_TOGGLE,
      DEFAULT_FILTERS_RESET,
    ];

    if (filterChangeActions.includes(action.type)) {
      filterChange({
        action,
        state,
        oldState,
        telemetry,
        sessionId,
      });
    }

    return res;
  };
}

function filterChange({action, state, oldState, telemetry, sessionId}) {
  const oldFilterState = oldState.filters;
  const filterState = state.filters;
  const activeFilters = [];
  const inactiveFilters = [];
  for (const [key, value] of Object.entries(filterState)) {
    if (value) {
      activeFilters.push(key);
    } else {
      inactiveFilters.push(key);
    }
  }

  let trigger;
  if (action.type === FILTER_TOGGLE) {
    trigger = action.filter;
  } else if (action.type === DEFAULT_FILTERS_RESET) {
    trigger = "reset";
  } else if (action.type === FILTER_TEXT_SET) {
    if (oldFilterState.text !== "" && filterState.text !== "") {
      return;
    }

    trigger = "text";
  }

  telemetry.recordEvent("filters_changed", "webconsole", null, {
    "trigger": trigger,
    "active": activeFilters.join(","),
    "inactive": inactiveFilters.join(","),
    "session_id": sessionId,
  });
}

module.exports = eventTelemetryMiddleware;
