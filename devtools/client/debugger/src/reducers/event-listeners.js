/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../utils/prefs";

export function initialEventListenerState() {
  return {
    // `categories` are shared by both breakpoint and tracer EventListeners panel
    categories: [],

    byPanel: {
      breakpoint: {
        active: [],
        expanded: [],
      },
      tracer: {
        // `active` is being handled by tracer-frames reducer
        expanded: [],
      },
    },

    logEventBreakpoints: prefs.logEventBreakpoints,

    // Firefox 132 changed the layout of the event listener data to support both breakpoints and tracer
    // Keep the old entries, which are stored in asyncStore in order to allow migrating from new to old profiles.
    // These aren't used, but only stored in asyncStore.
    active: [],
    expanded: [],
  };
}

function update(state = initialEventListenerState(), action) {
  switch (action.type) {
    case "RECEIVE_EVENT_LISTENER_TYPES":
      return { ...state, categories: action.categories };

    case "UPDATE_EVENT_LISTENERS":
      if (action.panelKey == "tracer") {
        return state;
      }
      state.byPanel[action.panelKey].active = action.active;
      return { ...state };

    case "UPDATE_EVENT_LISTENER_EXPANDED":
      state.byPanel[action.panelKey].expanded = action.expanded;
      return { ...state };

    case "TOGGLE_EVENT_LISTENERS": {
      const { logEventBreakpoints } = action;
      prefs.logEventBreakpoints = logEventBreakpoints;
      return { ...state, logEventBreakpoints };
    }

    default:
      return state;
  }
}

export default update;
