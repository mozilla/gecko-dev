/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function getActiveEventListeners(state, panelKey) {
  if (panelKey == "breakpoint") {
    return state.eventListenerBreakpoints.byPanel[panelKey].active;
  }
  return state.tracerFrames.activeDomEvents;
}

export function getEventListenerBreakpointTypes(state, panelKey) {
  if (panelKey == "breakpoint") {
    return state.eventListenerBreakpoints.categories;
  }
  return state.tracerFrames.domEventCategories;
}

export function getEventListenerExpanded(state, panelKey) {
  return state.eventListenerBreakpoints.byPanel[panelKey].expanded;
}

export function shouldLogEventBreakpoints(state) {
  return state.eventListenerBreakpoints.logEventBreakpoints;
}
