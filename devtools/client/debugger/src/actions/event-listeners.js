/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getActiveEventListeners,
  getEventListenerExpanded,
  shouldLogEventBreakpoints,
} from "../selectors/index";

async function updateBreakpoints(dispatch, client, panelKey, newEvents) {
  // Only breakpoints need to be communicated to the backend.
  // The Tracer is a 100% frontend data set.
  if (panelKey == "breakpoint") {
    await client.setEventListenerBreakpoints(newEvents);
  }
  dispatch({ type: "UPDATE_EVENT_LISTENERS", panelKey, active: newEvents });
}

async function updateExpanded(dispatch, panelKey, newExpanded) {
  dispatch({
    type: "UPDATE_EVENT_LISTENER_EXPANDED",
    panelKey,
    expanded: newExpanded,
  });
}

export function addEventListenerBreakpoints(panelKey, eventsToAdd) {
  return async ({ dispatch, client, getState }) => {
    const activeListenerBreakpoints = getActiveEventListeners(
      getState(),
      panelKey
    );

    const newEvents = [
      ...new Set([...eventsToAdd, ...activeListenerBreakpoints]),
    ];
    await updateBreakpoints(dispatch, client, panelKey, newEvents);
  };
}

export function removeEventListenerBreakpoints(panelKey, eventsToRemove) {
  return async ({ dispatch, client, getState }) => {
    const activeListenerBreakpoints = getActiveEventListeners(
      getState(),
      panelKey
    );

    const newEvents = activeListenerBreakpoints.filter(
      event => !eventsToRemove.includes(event)
    );

    await updateBreakpoints(dispatch, client, panelKey, newEvents);
  };
}

export function toggleEventLogging() {
  return async ({ dispatch, getState, client }) => {
    const logEventBreakpoints = !shouldLogEventBreakpoints(getState());
    await client.toggleEventLogging(logEventBreakpoints);
    dispatch({ type: "TOGGLE_EVENT_LISTENERS", logEventBreakpoints });
  };
}

export function addEventListenerExpanded(panelKey, category) {
  return async ({ dispatch, getState }) => {
    const expanded = await getEventListenerExpanded(getState(), panelKey);
    const newExpanded = [...new Set([...expanded, category])];
    await updateExpanded(dispatch, panelKey, newExpanded);
  };
}

export function removeEventListenerExpanded(panelKey, category) {
  return async ({ dispatch, getState }) => {
    const expanded = await getEventListenerExpanded(getState(), panelKey);

    const newExpanded = expanded.filter(expand => expand != category);

    updateExpanded(dispatch, panelKey, newExpanded);
  };
}

export function getEventListenerBreakpointTypes() {
  return async ({ dispatch, client }) => {
    const categories = await client.getEventListenerBreakpointTypes();
    dispatch({ type: "RECEIVE_EVENT_LISTENER_TYPES", categories });
  };
}
